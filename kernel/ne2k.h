#ifndef AIOS_NE2K_H
#define AIOS_NE2K_H

#include <stdint.h>

#define NE2K_REG_COMMAND 0x00U
#define NE2K_REG_RESET   0x1fU
#define NE2K_REG_DCR     0x0eU
#define NE2K_REG_ISR     0x07U
#define NE2K_COMMAND_STOP 0x01U
#define NE2K_COMMAND_PAGE0 0x00U
#define NE2K_COMMAND_PAGE1 0x40U
#define NE2K_DCR_WORD_MODE 0x49U
#define NE2K_ISR_RESET 0x80U

typedef uint8_t (*ne2k_inb_fn)(void* context, uint16_t port);
typedef void (*ne2k_outb_fn)(void* context, uint16_t port, uint8_t value);

typedef struct {
    void* context;
    ne2k_inb_fn inb;
    ne2k_outb_fn outb;
} ne2k_io_t;

typedef struct {
    uint16_t base_port;
    uint8_t initialized;
    uint8_t mac[6];
} ne2k_device_t;

/* Sonde le registre reset et prépare le mode arrêt/word pour une init ultérieure. */
int ne2k_probe(ne2k_device_t* device, uint16_t base_port, const ne2k_io_t* io);
/* Initialise les paramètres invariants du contrôleur sans allocation. */
int ne2k_prepare(ne2k_device_t* device, const ne2k_io_t* io);
/* Définit une MAC locale valide: non nulle et non multicast. */
int ne2k_set_mac(ne2k_device_t* device, const uint8_t mac[6]);

#endif
