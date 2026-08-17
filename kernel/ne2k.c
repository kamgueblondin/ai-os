#include "ne2k.h"

int ne2k_probe(ne2k_device_t* device, uint16_t base_port, const ne2k_io_t* io) {
    uint8_t reset_value;
    if (!device || !io || !io->inb || !io->outb || base_port == 0U) return -1;
    device->base_port = base_port;
    device->initialized = 0U;
    device->mac[0] = device->mac[1] = device->mac[2] = 0U;
    device->mac[3] = device->mac[4] = device->mac[5] = 0U;
    io->outb(io->context, (uint16_t)(base_port + NE2K_REG_COMMAND),
             NE2K_COMMAND_STOP | NE2K_COMMAND_PAGE0);
    reset_value = io->inb(io->context, (uint16_t)(base_port + NE2K_REG_RESET));
    io->outb(io->context, (uint16_t)(base_port + NE2K_REG_RESET), reset_value);
    if ((io->inb(io->context, (uint16_t)(base_port + NE2K_REG_ISR)) & NE2K_ISR_RESET) == 0U)
        return -2;
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
    return nonzero ? 0 : -2;
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
