#include "ne2k.h"

#ifdef __i386__
static uint8_t ne2k_i386_inb(void* context, uint16_t port) {
    uint8_t value; (void)context;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static void ne2k_i386_outb(void* context, uint16_t port, uint8_t value) {
    (void)context;
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
#endif

static ne2k_device_t* ne2k_irq_device;
static const ne2k_io_t* ne2k_irq_io;
static volatile uint32_t ne2k_irq_events;

int ne2k_tx_udp(ne2k_device_t* device, const ne2k_io_t* io,
                uint8_t* frame, uint16_t frame_capacity,
                const uint8_t destination_mac[6],
                const uint8_t source_ipv4[4], const uint8_t destination_ipv4[4],
                uint16_t source_port, uint16_t destination_port,
                const uint8_t* payload, uint16_t payload_length) {
    uint16_t ip_length;
    uint32_t total_length;
    uint8_t i;
    if (!device || !io || !frame || !destination_mac || !source_ipv4 ||
        !destination_ipv4 || (!payload && payload_length != 0U) ||
        !device->mac_valid) return -1;
    total_length = NET_ETHERNET_HEADER_SIZE + NET_IPV4_HEADER_SIZE +
                   NET_UDP_HEADER_SIZE + payload_length;
    if (total_length > frame_capacity || total_length > NE2K_ETHERNET_MAX_FRAME)
        return -2;
    for (i = 0; i < 6U; ++i) { frame[i] = destination_mac[i]; frame[6U + i] = device->mac[i]; }
    frame[12] = (uint8_t)(NET_ETHERTYPE_IPV4 >> 8);
    frame[13] = (uint8_t)NET_ETHERTYPE_IPV4;
    ip_length = (uint16_t)net_udp_build_ipv4(frame + NET_ETHERNET_HEADER_SIZE,
                                             frame_capacity - NET_ETHERNET_HEADER_SIZE,
                                             source_ipv4, destination_ipv4,
                                             source_port, destination_port,
                                             payload, payload_length);
    if (ip_length == 0U) return -3;
    return ne2k_tx_submit(device, io, frame, (uint16_t)(NET_ETHERNET_HEADER_SIZE + ip_length));
}

int ne2k_arp_resolve(ne2k_device_t* device, const ne2k_io_t* io,
                     net_arp_cache_t* cache,
                     uint8_t* request_frame, uint16_t request_capacity,
                     uint8_t* rx_frame, uint16_t rx_capacity,
                     const uint8_t local_mac[6], const uint8_t local_ipv4[4],
                     const uint8_t target_ipv4[4], uint16_t attempts) {
    uint8_t destination_mac[6]; uint16_t request_length, rx_length, i;
    net_ethernet_header_t ethernet; net_arp_packet_t arp; int status;
    if (!device || !io || !cache || !request_frame || !rx_frame || !local_mac ||
        !local_ipv4 || !target_ipv4 || attempts == 0U) return -1;
    if (net_arp_cache_lookup(cache, target_ipv4, destination_mac) == 0) return 0;
    request_length = (uint16_t)net_arp_build_request(request_frame, request_capacity,
                                                      local_mac, local_ipv4, target_ipv4);
    if (request_length == 0U) return -2;
    status = ne2k_tx_submit(device, io, request_frame, request_length);
    if (status != 0) return -3;
    for (i = 0; i < attempts; ++i) {
        status = ne2k_rx_poll_arp(device, io, rx_frame, rx_capacity, &rx_length,
                                  &ethernet, &arp);
        if (status == 1) continue;
        if (status != 0) continue;
        if (net_arp_is_reply_for(&arp, local_ipv4, target_ipv4)) {
            if (net_arp_cache_put(cache, target_ipv4, arp.sender_mac) != 0) return -4;
            return 0;
        }
    }
    return -5;
}

int ne2k_tx_udp_resolve(ne2k_device_t* device, const ne2k_io_t* io,
                        net_arp_cache_t* cache,
                        uint8_t* request_frame, uint16_t request_capacity,
                        uint8_t* rx_frame, uint16_t rx_capacity,
                        uint8_t* tx_frame, uint16_t tx_capacity,
                        const uint8_t local_ipv4[4], const uint8_t target_ipv4[4],
                        uint16_t source_port, uint16_t destination_port,
                        const uint8_t* payload, uint16_t payload_length,
                        uint16_t attempts) {
    uint8_t destination_mac[6]; int status;
    status = ne2k_arp_resolve(device, io, cache, request_frame, request_capacity,
                              rx_frame, rx_capacity, device ? device->mac : 0,
                              local_ipv4, target_ipv4, attempts);
    if (status != 0) return status;
    if (net_arp_cache_lookup(cache, target_ipv4, destination_mac) != 0) return -6;
    return ne2k_tx_udp(device, io, tx_frame, tx_capacity, destination_mac,
                       local_ipv4, target_ipv4, source_port, destination_port,
                       payload, payload_length);
}

int ne2k_dhcp_discover(ne2k_device_t* device, const ne2k_io_t* io,
                       uint8_t* frame, uint16_t frame_capacity, uint32_t xid) {
    static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static const uint8_t zero_ip[4] = {0, 0, 0, 0};
    static const uint8_t broadcast_ip[4] = {255, 255, 255, 255};
    int payload_length;
    if (!device || !io || !frame ||
        frame_capacity < NET_ETHERNET_HEADER_SIZE + NET_IPV4_HEADER_SIZE +
                          NET_UDP_HEADER_SIZE + 244U)
        return -1;
    payload_length = net_dhcp_build_discover(frame + NET_ETHERNET_HEADER_SIZE +
                                              NET_IPV4_HEADER_SIZE + NET_UDP_HEADER_SIZE,
                                              frame_capacity - NET_ETHERNET_HEADER_SIZE -
                                              NET_IPV4_HEADER_SIZE - NET_UDP_HEADER_SIZE,
                                              xid, device->mac);
    if (payload_length < 0) return -2;
    return ne2k_tx_udp(device, io, frame, frame_capacity, broadcast_mac,
                       zero_ip, broadcast_ip, 68U, 67U,
                       frame + NET_ETHERNET_HEADER_SIZE + NET_IPV4_HEADER_SIZE +
                       NET_UDP_HEADER_SIZE, (uint16_t)payload_length);
}

int ne2k_dhcp_request(ne2k_device_t* device, const ne2k_io_t* io,
                      uint8_t* frame, uint16_t frame_capacity,
                      uint32_t xid, const uint8_t requested_ip[4],
                      const uint8_t server_ip[4]) {
    static const uint8_t broadcast_mac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
    static const uint8_t zero_ip[4] = {0,0,0,0};
    static const uint8_t broadcast_ip[4] = {255,255,255,255};
    int payload_length;
    if (!device || !io || !frame || !requested_ip || !server_ip ||
        frame_capacity < NET_ETHERNET_HEADER_SIZE + NET_IPV4_HEADER_SIZE + NET_UDP_HEADER_SIZE + 255U)
        return -1;
    payload_length = net_dhcp_build_request(frame + NET_ETHERNET_HEADER_SIZE + NET_IPV4_HEADER_SIZE + NET_UDP_HEADER_SIZE,
        frame_capacity - NET_ETHERNET_HEADER_SIZE - NET_IPV4_HEADER_SIZE - NET_UDP_HEADER_SIZE,
        xid, device->mac, requested_ip, server_ip);
    if (payload_length < 0) return -2;
    return ne2k_tx_udp(device, io, frame, frame_capacity, broadcast_mac, zero_ip, broadcast_ip,
                       68U, 67U, frame + NET_ETHERNET_HEADER_SIZE + NET_IPV4_HEADER_SIZE + NET_UDP_HEADER_SIZE,
                       (uint16_t)payload_length);
}

int ne2k_dns_query(ne2k_device_t* device, const ne2k_io_t* io,
                   net_arp_cache_t* cache, uint8_t* arp_request, uint16_t arp_request_capacity,
                   uint8_t* arp_rx, uint16_t arp_rx_capacity, uint8_t* frame, uint16_t frame_capacity,
                   const uint8_t local_ip[4], const uint8_t dns_ip[4], uint16_t id,
                   const char* hostname) {
    int payload_length;
    if (!device || !io || !cache || !arp_request || !arp_rx || !frame || !local_ip || !dns_ip || !hostname ||
        frame_capacity < NET_ETHERNET_HEADER_SIZE + NET_IPV4_HEADER_SIZE + NET_UDP_HEADER_SIZE + 12U)
        return -1;
    payload_length = net_dns_build_a_query(frame + NET_ETHERNET_HEADER_SIZE + NET_IPV4_HEADER_SIZE + NET_UDP_HEADER_SIZE,
        frame_capacity - NET_ETHERNET_HEADER_SIZE - NET_IPV4_HEADER_SIZE - NET_UDP_HEADER_SIZE, id, hostname);
    if (payload_length < 0) return -2;
    return ne2k_tx_udp_resolve(device, io, cache, arp_request, arp_request_capacity, arp_rx, arp_rx_capacity,
        frame, frame_capacity, local_ip, dns_ip, 49152U, 53U,
        frame + NET_ETHERNET_HEADER_SIZE + NET_IPV4_HEADER_SIZE + NET_UDP_HEADER_SIZE,
        (uint16_t)payload_length, 8U);
}

int ne2k_tcp_syn(ne2k_device_t* device, const ne2k_io_t* io,
                 net_arp_cache_t* cache, uint8_t* arp_request, uint16_t arp_request_capacity,
                 uint8_t* arp_rx, uint16_t arp_rx_capacity, uint8_t* frame, uint16_t frame_capacity,
                 const uint8_t local_ip[4], const uint8_t remote_ip[4],
                 uint16_t local_port, uint16_t remote_port, uint32_t sequence) {
    int tcp_length;
    if (!device || !io || !cache || !arp_request || !arp_rx || !frame || !local_ip || !remote_ip ||
        frame_capacity < NET_ETHERNET_HEADER_SIZE + 40U) return -1;
    tcp_length = net_tcp_build_syn_ipv4(frame + NET_ETHERNET_HEADER_SIZE,
        frame_capacity - NET_ETHERNET_HEADER_SIZE, local_ip, remote_ip, local_port, remote_port, sequence);
    if (tcp_length < 0) return -2;
    { uint8_t destination_mac[6]; uint16_t i;
      if (net_arp_cache_lookup(cache, remote_ip, destination_mac) != 0) return -3;
      for (i = 0; i < 6U; ++i) { frame[i] = destination_mac[i]; frame[6U+i] = device->mac[i]; }
      frame[12] = 0x08U; frame[13] = 0x00U;
      return ne2k_tx_submit(device, io, frame, (uint16_t)(NET_ETHERNET_HEADER_SIZE + tcp_length)); }
}

int ne2k_tcp_ack(ne2k_device_t* device, const ne2k_io_t* io,
                 const net_arp_cache_t* cache, uint8_t* frame, uint16_t frame_capacity,
                 const uint8_t local_ip[4], const uint8_t remote_ip[4],
                 const net_tcp_connection_t* connection) {
    uint8_t destination_mac[6]; uint16_t tcp_length, i; uint32_t sum = 0U;
    if (!device || !io || !cache || !frame || !local_ip || !remote_ip || !connection ||
        !device->mac_valid || frame_capacity < NET_ETHERNET_HEADER_SIZE + 40U) return -1;
    if (net_arp_cache_lookup(cache, remote_ip, destination_mac) != 0) return -2;
    for (i = 0; i < 6U; ++i) { frame[i] = destination_mac[i]; frame[6U+i] = device->mac[i]; }
    frame[12] = 0x08U; frame[13] = 0x00U;
    for (i = 0; i < 40U; ++i) frame[NET_ETHERNET_HEADER_SIZE+i] = 0U;
    frame[NET_ETHERNET_HEADER_SIZE] = 0x45U;
    frame[NET_ETHERNET_HEADER_SIZE+2U] = 0U; frame[NET_ETHERNET_HEADER_SIZE+3U] = 40U;
    frame[NET_ETHERNET_HEADER_SIZE+8U] = 64U; frame[NET_ETHERNET_HEADER_SIZE+9U] = NET_TCP_PROTOCOL;
    for (i = 0; i < 4U; ++i) { frame[NET_ETHERNET_HEADER_SIZE+12U+i] = local_ip[i]; frame[NET_ETHERNET_HEADER_SIZE+16U+i] = remote_ip[i]; }
    tcp_length = (uint16_t)net_tcp_connection_build_ack(connection, frame + NET_ETHERNET_HEADER_SIZE + 20U,
                                                         frame_capacity - NET_ETHERNET_HEADER_SIZE - 20U);
    if ((int16_t)tcp_length < 0) return -3;
    /* Le segment TCP est construit dans la zone caller-owned; calculer son checksum. */
    frame[NET_ETHERNET_HEADER_SIZE+36U] = 0U; frame[NET_ETHERNET_HEADER_SIZE+37U] = 0U;
    { uint16_t checksum = net_tcp_checksum_ipv4(local_ip, remote_ip, frame + NET_ETHERNET_HEADER_SIZE + 20U, tcp_length);
      frame[NET_ETHERNET_HEADER_SIZE+36U] = (uint8_t)(checksum >> 8); frame[NET_ETHERNET_HEADER_SIZE+37U] = (uint8_t)checksum; }
    for (i = 0; i < 20U; i += 2U) { sum += ((uint16_t)frame[NET_ETHERNET_HEADER_SIZE+i] << 8) | frame[NET_ETHERNET_HEADER_SIZE+i+1U]; while (sum >> 16) sum = (sum & 0xffffU) + (sum >> 16); }
    sum = (~sum) & 0xffffU; frame[NET_ETHERNET_HEADER_SIZE+10U] = (uint8_t)(sum >> 8); frame[NET_ETHERNET_HEADER_SIZE+11U] = (uint8_t)sum;
    return ne2k_tx_submit(device, io, frame, (uint16_t)(NET_ETHERNET_HEADER_SIZE + 40U));
}

int ne2k_tcp_fin(ne2k_device_t* device, const ne2k_io_t* io,
                 const net_arp_cache_t* cache, uint8_t* frame, uint16_t frame_capacity,
                 const uint8_t local_ip[4], const uint8_t remote_ip[4],
                 net_tcp_connection_t* connection) {
    uint8_t destination_mac[6]; uint16_t i; uint32_t sum = 0U; int tcp_length, status; net_tcp_connection_t next;
    if (!device || !io || !cache || !frame || !local_ip || !remote_ip || !connection || !device->mac_valid ||
        frame_capacity < NET_ETHERNET_HEADER_SIZE + 40U || connection->state != NET_TCP_STATE_ESTABLISHED) return -1;
    if (net_arp_cache_lookup(cache, remote_ip, destination_mac) != 0) return -2;
    for (i = 0; i < 6U; ++i) { frame[i] = destination_mac[i]; frame[6U+i] = device->mac[i]; }
    frame[12] = 0x08U; frame[13] = 0x00U;
    for (i = 0; i < 40U; ++i) frame[NET_ETHERNET_HEADER_SIZE+i] = 0U;
    frame[14] = 0x45U; frame[16] = 0U; frame[17] = 40U; frame[22] = 64U; frame[23] = NET_TCP_PROTOCOL;
    for (i = 0; i < 4U; ++i) { frame[26U+i] = local_ip[i]; frame[30U+i] = remote_ip[i]; }
    next = *connection;
    tcp_length = net_tcp_connection_begin_close(&next, frame + 34U, frame_capacity - 34U);
    if (tcp_length < 0) return -3;
    { uint16_t checksum = net_tcp_checksum_ipv4(local_ip, remote_ip, frame + 34U, (uint16_t)tcp_length);
      frame[50U] = (uint8_t)(checksum >> 8); frame[51U] = (uint8_t)checksum; }
    for (i = 0; i < 20U; i += 2U) { sum += ((uint16_t)frame[14U+i] << 8) | frame[15U+i]; while (sum >> 16) sum = (sum & 0xffffU) + (sum >> 16); }
    sum = (~sum) & 0xffffU; frame[24] = (uint8_t)(sum >> 8); frame[25] = (uint8_t)sum;
    status = ne2k_tx_submit(device, io, frame, NET_ETHERNET_HEADER_SIZE + 40U);
    if (status != 0) return status;
    *connection = next;
    return 0;
}

int ne2k_tcp_data(ne2k_device_t* device, const ne2k_io_t* io,
                  const net_arp_cache_t* cache, uint8_t* frame, uint16_t frame_capacity,
                  const uint8_t local_ip[4], const uint8_t remote_ip[4],
                  const net_tcp_connection_t* connection, const uint8_t* payload,
                  uint16_t payload_length) {
    uint8_t destination_mac[6]; uint16_t tcp_length, i, ip_length; uint32_t sum = 0U;
    if (!device || !io || !cache || !frame || !local_ip || !remote_ip || !connection ||
        (!payload && payload_length != 0U) || !device->mac_valid) return -1;
    if (net_arp_cache_lookup(cache, remote_ip, destination_mac) != 0) return -2;
    ip_length = (uint16_t)(20U + NET_TCP_HEADER_SIZE + payload_length);
    if ((uint32_t)NET_ETHERNET_HEADER_SIZE + ip_length > frame_capacity ||
        (uint32_t)NET_ETHERNET_HEADER_SIZE + ip_length > NE2K_ETHERNET_MAX_FRAME) return -3;
    for (i = 0; i < 6U; ++i) { frame[i] = destination_mac[i]; frame[6U+i] = device->mac[i]; }
    frame[12] = 0x08U; frame[13] = 0x00U;
    for (i = 0; i < ip_length; ++i) frame[NET_ETHERNET_HEADER_SIZE+i] = 0U;
    frame[14] = 0x45U; frame[16] = (uint8_t)(ip_length >> 8); frame[17] = (uint8_t)ip_length;
    frame[22] = 64U; frame[23] = NET_TCP_PROTOCOL;
    for (i = 0; i < 4U; ++i) { frame[26U+i] = local_ip[i]; frame[30U+i] = remote_ip[i]; }
    tcp_length = (uint16_t)net_tcp_build_data(frame + 34U, frame_capacity - 34U,
                                               connection->local_port, connection->remote_port,
                                               connection->local_sequence, connection->remote_sequence,
                                               payload, payload_length);
    if ((int16_t)tcp_length < 0) return -4;
    { uint16_t checksum = net_tcp_checksum_ipv4(local_ip, remote_ip, frame + 34U, tcp_length);
      frame[50U] = (uint8_t)(checksum >> 8); frame[51U] = (uint8_t)checksum; }
    for (i = 0; i < 20U; i += 2U) { sum += ((uint16_t)frame[14U+i] << 8) | frame[15U+i]; while (sum >> 16) sum = (sum & 0xffffU) + (sum >> 16); }
    sum = (~sum) & 0xffffU; frame[24] = (uint8_t)(sum >> 8); frame[25] = (uint8_t)sum;
    return ne2k_tx_submit(device, io, frame, (uint16_t)(NET_ETHERNET_HEADER_SIZE + ip_length));
}

int ne2k_tcp_retransmit(ne2k_device_t* device, const ne2k_io_t* io,
                        const net_arp_cache_t* cache, uint8_t* frame, uint16_t frame_capacity,
                        const uint8_t local_ip[4], const uint8_t remote_ip[4],
                        net_tcp_connection_t* connection) {
    net_tcp_connection_t retransmit_view; int status;
    if (!connection || !net_tcp_connection_retransmit_allowed(connection)) return -1;
    retransmit_view = *connection;
    retransmit_view.local_sequence = connection->local_sequence - connection->pending_length;
    status = ne2k_tcp_data(device, io, cache, frame, frame_capacity, local_ip, remote_ip,
                           &retransmit_view, connection->pending_payload, connection->pending_length);
    if (status != 0) return status;
    return net_tcp_connection_note_retransmit(connection);
}

int ne2k_dns_poll_a(ne2k_device_t* device, const ne2k_io_t* io,
                    uint8_t* frame, uint16_t frame_capacity, uint16_t attempts,
                    uint16_t expected_id, net_dns_a_result_t* result) {
    uint16_t frame_length, i; net_udp_view_t udp; int status;
    if (!device || !io || !frame || !result || attempts == 0U) return -1;
    for (i = 0; i < attempts; ++i) {
        status = ne2k_rx_poll_udp(device, io, frame, frame_capacity, &frame_length, &udp);
        if (status != 0) continue;
        if (udp.source_port != 53U || udp.destination_port != 49152U) continue;
        if (net_dns_parse_a_response(udp.payload, udp.payload_length, expected_id, result) == 0) return 0;
    }
    return -2;
}

int ne2k_dhcp_poll_offer(ne2k_device_t* device, const ne2k_io_t* io,
                         uint8_t* frame, uint16_t frame_capacity,
                         uint32_t expected_xid, uint16_t attempts,
                         net_dhcp_offer_t* offer) {
    uint16_t frame_length, i; net_udp_view_t udp; int status;
    if (!device || !io || !frame || !offer || attempts == 0U) return -1;
    for (i = 0; i < attempts; ++i) {
        status = ne2k_rx_poll_udp(device, io, frame, frame_capacity,
                                  &frame_length, &udp);
        if (status == 1) continue;
        if (status != 0) continue;
        if (udp.source_port != 67U || udp.destination_port != 68U ||
            udp.payload_length < 244U) continue;
        status = net_dhcp_parse_offer(udp.payload, udp.payload_length,
                                      expected_xid, offer);
        if (status == 0) return 0;
    }
    return -2;
}

int ne2k_irq_attach(ne2k_device_t* device, const ne2k_io_t* io) {
    if (!device || !io || !io->inb || !io->outb || device->base_port == 0U)
        return -1;
    ne2k_irq_device = device;
    ne2k_irq_io = io;
    ne2k_irq_events = 0U;
    return 0;
}

void ne2k_irq_service(void) {
    uint8_t status;
    if (!ne2k_irq_device || !ne2k_irq_io) return;
    status = ne2k_irq_io->inb(ne2k_irq_io->context,
                              (uint16_t)(ne2k_irq_device->base_port + NE2K_REG_ISR));
    if (status == 0U) return;
    ne2k_irq_io->outb(ne2k_irq_io->context,
                      (uint16_t)(ne2k_irq_device->base_port + NE2K_REG_ISR), status);
    ++ne2k_irq_events;
}

uint32_t ne2k_irq_count(void) {
    return ne2k_irq_events;
}

static void ne2k_remote_read_setup(const ne2k_io_t* io, uint16_t base,
                                   uint16_t address, uint16_t length) {
    io->outb(io->context, (uint16_t)(base + NE2K_REG_RBCR0), (uint8_t)length);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_RBCR1), (uint8_t)(length >> 8));
    io->outb(io->context, (uint16_t)(base + NE2K_REG_RSAR0), (uint8_t)address);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_RSAR1), (uint8_t)(address >> 8));
    io->outb(io->context, (uint16_t)(base + NE2K_REG_COMMAND),
             NE2K_COMMAND_REMOTE_READ);
}

int ne2k_rx_poll(ne2k_device_t* device, const ne2k_io_t* io,
                 uint8_t* frame, uint16_t frame_capacity,
                 uint16_t* frame_length) {
    uint16_t base, page, packet_length, payload_length;
    uint8_t header[NE2K_RX_HEADER_SIZE];
    uint8_t next_page;
    uint32_t i;
    if (!device || !io || !io->inb || !io->outb || !frame || !frame_length ||
        !device->initialized || frame_capacity == 0U || device->base_port == 0U)
        return -1;
    *frame_length = 0U;
    base = device->base_port;
    if ((io->inb(io->context, (uint16_t)(base + NE2K_REG_ISR)) & NE2K_ISR_PRX) == 0U)
        return 1;
    page = (uint16_t)io->inb(io->context, (uint16_t)(base + NE2K_REG_BNRY)) + 1U;
    if (page >= NE2K_RX_PAGE_STOP) page = NE2K_RX_PAGE_START;
    ne2k_remote_read_setup(io, base, (uint16_t)(page << 8), NE2K_RX_HEADER_SIZE);
    for (i = 0; i < NE2K_RX_HEADER_SIZE; ++i)
        header[i] = io->inb(io->context, (uint16_t)(base + NE2K_REG_DATA));
    if ((header[0] & NE2K_RX_STATUS_OK) == 0U) return -2;
    next_page = header[1];
    packet_length = (uint16_t)(header[2] | ((uint16_t)header[3] << 8));
    if (packet_length < NE2K_RX_HEADER_SIZE || packet_length > NE2K_ETHERNET_MAX_FRAME)
        return -3;
    payload_length = (uint16_t)(packet_length - NE2K_RX_HEADER_SIZE);
    if (payload_length > frame_capacity) return -4;
    ne2k_remote_read_setup(io, base, (uint16_t)((page << 8) + NE2K_RX_HEADER_SIZE),
                           payload_length);
    for (i = 0; i < payload_length; ++i)
        frame[i] = io->inb(io->context, (uint16_t)(base + NE2K_REG_DATA));
    if (next_page < NE2K_RX_PAGE_START || next_page >= NE2K_RX_PAGE_STOP)
        return -5;
    io->outb(io->context, (uint16_t)(base + NE2K_REG_BNRY),
             (uint8_t)(next_page == NE2K_RX_PAGE_START ? NE2K_RX_PAGE_STOP - 1U : next_page - 1U));
    *frame_length = payload_length;
    return 0;
}

int ne2k_rx_poll_arp(ne2k_device_t* device, const ne2k_io_t* io,
                     uint8_t* frame, uint16_t frame_capacity,
                     uint16_t* frame_length,
                     net_ethernet_header_t* ethernet,
                     net_arp_packet_t* arp) {
    int status;
    if (!ethernet || !arp) return -1;
    status = ne2k_rx_poll(device, io, frame, frame_capacity, frame_length);
    if (status != 0) return status;
    if (net_ethernet_parse(frame, *frame_length, ethernet) != 0)
        return -2;
    if (ethernet->ethertype != NET_ETHERTYPE_ARP)
        return 2;
    if (net_arp_parse(frame, *frame_length, arp) != 0)
        return -3;
    return 0;
}

int ne2k_arp_service(ne2k_device_t* device, const ne2k_io_t* io,
                     uint8_t* rx_frame, uint16_t rx_capacity,
                     uint8_t* tx_frame, uint16_t tx_capacity,
                     const uint8_t local_mac[6], const uint8_t local_ipv4[4]) {
    uint16_t rx_length = 0U;
    net_ethernet_header_t ethernet;
    net_arp_packet_t arp;
    int status;
    if (!tx_frame || !local_mac || !local_ipv4) return -1;
    status = ne2k_rx_poll_arp(device, io, rx_frame, rx_capacity, &rx_length,
                              &ethernet, &arp);
    if (status != 0) return status;
    if (arp.opcode != NET_ARP_OPCODE_REQUEST ||
        arp.target_ipv4[0] != local_ipv4[0] || arp.target_ipv4[1] != local_ipv4[1] ||
        arp.target_ipv4[2] != local_ipv4[2] || arp.target_ipv4[3] != local_ipv4[3])
        return 2;
    status = net_arp_build_reply(tx_frame, tx_capacity, &arp, local_mac, local_ipv4);
    if (status < 0) return -2;
    return ne2k_tx_submit(device, io, tx_frame, (uint16_t)status);
}

int ne2k_rx_poll_tcp(ne2k_device_t* device, const ne2k_io_t* io,
                     uint8_t* frame, uint16_t frame_capacity,
                     uint16_t* frame_length, net_tcp_view_t* tcp) {
    net_ethernet_header_t ethernet; uint16_t ip_header_size; int status;
    if (!tcp) return -1;
    status = ne2k_rx_poll(device, io, frame, frame_capacity, frame_length);
    if (status != 0) return status;
    if (net_ethernet_parse(frame, *frame_length, &ethernet) != 0) return -2;
    if (ethernet.ethertype != NET_ETHERTYPE_IPV4) return 2;
    if (*frame_length < NET_ETHERNET_HEADER_SIZE + 20U) return -3;
    ip_header_size = (uint16_t)(frame[NET_ETHERNET_HEADER_SIZE] & 0x0fU) * 4U;
    if (ip_header_size < 20U || frame[NET_ETHERNET_HEADER_SIZE + 9U] != NET_TCP_PROTOCOL ||
        *frame_length < NET_ETHERNET_HEADER_SIZE + ip_header_size) return -3;
    if (net_tcp_parse(frame + NET_ETHERNET_HEADER_SIZE + ip_header_size,
                      (uint32_t)(*frame_length - NET_ETHERNET_HEADER_SIZE - ip_header_size), tcp) != 0)
        return -4;
    return 0;
}

int ne2k_rx_poll_udp(ne2k_device_t* device, const ne2k_io_t* io,
                     uint8_t* frame, uint16_t frame_capacity,
                     uint16_t* frame_length, net_udp_view_t* udp) {
    net_ethernet_header_t ethernet;
    int status;
    if (!udp) return -1;
    status = ne2k_rx_poll(device, io, frame, frame_capacity, frame_length);
    if (status != 0) return status;
    if (net_ethernet_parse(frame, *frame_length, &ethernet) != 0)
        return -2;
    if (ethernet.ethertype != NET_ETHERTYPE_IPV4)
        return 2;
    if (*frame_length <= NET_ETHERNET_HEADER_SIZE ||
        net_udp_parse_ipv4(frame + NET_ETHERNET_HEADER_SIZE,
                           (uint32_t)(*frame_length - NET_ETHERNET_HEADER_SIZE), udp) != 0)
        return -3;
    return 0;
}

int ne2k_i386_io(ne2k_io_t* io) {
    if (!io) return -1;
#ifdef __i386__
    io->context = (void*)0; io->inb = ne2k_i386_inb; io->outb = ne2k_i386_outb; return 0;
#else
    io->context = (void*)0; io->inb = (ne2k_inb_fn)0; io->outb = (ne2k_outb_fn)0; return -1;
#endif
}

int ne2k_probe(ne2k_device_t* device, uint16_t base_port, const ne2k_io_t* io) {
    uint8_t reset_value;
    uint8_t isr_value;
    if (!device || !io || !io->inb || !io->outb || base_port == 0U) return -1;
    device->base_port = base_port;
    device->initialized = 0U;
    device->mac[0] = device->mac[1] = device->mac[2] = 0U;
    device->mac[3] = device->mac[4] = device->mac[5] = 0U;
    device->mac_valid = 0U;
    io->outb(io->context, (uint16_t)(base_port + NE2K_REG_COMMAND),
             NE2K_COMMAND_STOP | NE2K_COMMAND_PAGE0);
    reset_value = io->inb(io->context, (uint16_t)(base_port + NE2K_REG_RESET));
    io->outb(io->context, (uint16_t)(base_port + NE2K_REG_RESET), reset_value);
    isr_value = io->inb(io->context, (uint16_t)(base_port + NE2K_REG_ISR));
    if ((isr_value & NE2K_ISR_RESET) == 0U)
        return -2;
    io->outb(io->context, (uint16_t)(base_port + NE2K_REG_DCR), NE2K_DCR_WORD_MODE);
    /* QEMU ne relit pas le DCR sur ce modèle; les ports flottants renvoient 0xff. */
    if (reset_value == 0xffU || isr_value == 0xffU) return -3;
    return 0;
}

int ne2k_configure_rings(ne2k_device_t* device, const ne2k_io_t* io) {
    uint16_t base;
    if (!device || !io || !io->outb || device->base_port == 0U) return -1;
    base = device->base_port;
    io->outb(io->context, (uint16_t)(base + NE2K_REG_COMMAND),
             NE2K_COMMAND_STOP | NE2K_COMMAND_PAGE0);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_TPSR), 0x40U);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_PSTART), 0x46U);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_PSTOP), 0x60U);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_BNRY), 0x46U);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_RCR), 0x04U);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_TCR), 0x00U);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_COMMAND), 0x22U);
    return 0;
}

int ne2k_set_mac(ne2k_device_t* device, const uint8_t mac[6]) {
    uint32_t i;
    uint8_t nonzero = 0U;
    if (!device || !mac || (mac[0] & 1U) != 0U) return -1;
    for (i = 0; i < 6U; ++i) {
        device->mac[i] = mac[i];
        if (mac[i] != 0U) nonzero = 1U;
    }
    device->mac_valid = nonzero;
    return nonzero ? 0 : -2;
}

int ne2k_read_mac(ne2k_device_t* device, const ne2k_io_t* io) {
    uint8_t prom[12];
    uint16_t base;
    uint32_t i;
    if (!device || !io || !io->inb || !io->outb || device->base_port == 0U)
        return -1;
    base = device->base_port;
    /* La PROM NE2000 expose la MAC sur les octets pairs d’une lecture 16 bits. */
    io->outb(io->context, (uint16_t)(base + NE2K_REG_COMMAND),
             NE2K_COMMAND_STOP | NE2K_COMMAND_PAGE0);
    for (i = 0; i < 12U; ++i)
        prom[i] = io->inb(io->context, (uint16_t)(base + NE2K_REG_DATA));
    for (i = 0; i < 6U; ++i)
        device->mac[i] = prom[i * 2U];
    device->mac_valid = 0U;
    for (i = 0; i < 6U; ++i)
        if (device->mac[i] != 0U) device->mac_valid = 1U;
    if ((device->mac[0] & 1U) != 0U || device->mac_valid == 0U) {
        device->mac_valid = 0U;
        return -2;
    }
    return 0;
}

int ne2k_rx_extract(const uint8_t* dma_buffer, uint16_t dma_length,
                    net_nic_queue_t* rx_queue) {
    uint16_t packet_length;
    uint8_t* destination;
    uint16_t capacity;
    if (!dma_buffer || !rx_queue || dma_length < NE2K_RX_HEADER_SIZE)
        return -1;
    if ((dma_buffer[0] & NE2K_RX_STATUS_OK) == 0U) return -2;
    packet_length = (uint16_t)(dma_buffer[2] | ((uint16_t)dma_buffer[3] << 8));
    if (packet_length < NE2K_RX_HEADER_SIZE || packet_length > dma_length)
        return -3;
    if (net_nic_queue_acquire(rx_queue, &destination, &capacity) != 0 ||
        packet_length - NE2K_RX_HEADER_SIZE > capacity)
        return -4;
    {
        uint16_t i;
        for (i = 0; i < packet_length - NE2K_RX_HEADER_SIZE; ++i)
            destination[i] = dma_buffer[NE2K_RX_HEADER_SIZE + i];
    }
    return net_nic_queue_commit(rx_queue,
                                 (uint16_t)(packet_length - NE2K_RX_HEADER_SIZE));
}

int ne2k_tx_submit(ne2k_device_t* device, const ne2k_io_t* io,
                   const uint8_t* frame, uint16_t length) {
    uint16_t base;
    uint16_t wire_length;
    uint32_t i;
    if (!device || !io || !io->inb || !io->outb || !frame ||
        !device->initialized || device->base_port == 0U || length == 0U ||
        length > NE2K_ETHERNET_MAX_FRAME)
        return -1;
    base = device->base_port;
    wire_length = length < NE2K_ETHERNET_MIN_FRAME ? NE2K_ETHERNET_MIN_FRAME : length;
    io->outb(io->context, (uint16_t)(base + NE2K_REG_COMMAND),
             NE2K_COMMAND_STOP | NE2K_COMMAND_PAGE0);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_DCR), NE2K_DCR_BYTE_MODE);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_RBCR0), (uint8_t)wire_length);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_RBCR1), (uint8_t)(wire_length >> 8));
    io->outb(io->context, (uint16_t)(base + NE2K_REG_RSAR0), 0U);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_RSAR1), NE2K_TX_PAGE);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_COMMAND), NE2K_COMMAND_REMOTE_WRITE);
    for (i = 0; i < (uint32_t)wire_length; ++i)
        io->outb(io->context, (uint16_t)(base + NE2K_REG_DATA),
                 i < length ? frame[i] : 0U);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_ISR), NE2K_ISR_RDC);
    for (i = 0; i < 65535U; ++i)
        if ((io->inb(io->context, (uint16_t)(base + NE2K_REG_ISR)) & NE2K_ISR_RDC) != 0U)
            break;
    if (i == 65535U) return -2;
    io->outb(io->context, (uint16_t)(base + NE2K_REG_TPSR), NE2K_TX_PAGE);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_TBCR0), (uint8_t)wire_length);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_TBCR1), (uint8_t)(wire_length >> 8));
    io->outb(io->context, (uint16_t)(base + NE2K_REG_COMMAND), NE2K_COMMAND_TRANSMIT);
    return 0;
}

int ne2k_prepare(ne2k_device_t* device, const ne2k_io_t* io) {
    uint16_t base;
    if (!device || !io || !io->outb || device->base_port == 0U) return -1;
    base = device->base_port;
    io->outb(io->context, (uint16_t)(base + NE2K_REG_COMMAND),
             NE2K_COMMAND_STOP | NE2K_COMMAND_PAGE0);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_DCR), NE2K_DCR_WORD_MODE);
    io->outb(io->context, (uint16_t)(base + NE2K_REG_ISR), 0xffU);
    device->initialized = 1U;
    return 0;
}
