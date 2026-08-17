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
