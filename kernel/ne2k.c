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
