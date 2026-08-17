#ifndef AIOS_NE2K_H
#define AIOS_NE2K_H

#include <stdint.h>
#include "net_nic.h"
#include "net_ethernet_arp.h"

#define NE2K_REG_COMMAND 0x00U
#define NE2K_REG_RESET   0x1fU
#define NE2K_REG_DCR     0x0eU
#define NE2K_REG_ISR     0x07U
#define NE2K_REG_TPSR    0x04U
#define NE2K_REG_PSTART  0x01U
#define NE2K_REG_PSTOP   0x02U
#define NE2K_REG_BNRY    0x03U
#define NE2K_REG_RCR     0x0cU
#define NE2K_REG_TCR    0x0dU
#define NE2K_REG_DATA   0x10U
#define NE2K_REG_TBCR0  0x05U
#define NE2K_REG_TBCR1  0x06U
#define NE2K_REG_RBCR0  0x0aU
#define NE2K_REG_RBCR1  0x0bU
#define NE2K_REG_RSAR0  0x08U
#define NE2K_REG_RSAR1  0x09U

#define NE2K_REG_CURR    0x07U
#define NE2K_COMMAND_STOP 0x01U
#define NE2K_COMMAND_PAGE0 0x00U
#define NE2K_COMMAND_PAGE1 0x40U
#define NE2K_DCR_WORD_MODE 0x49U
#define NE2K_ISR_RESET 0x80U
#define NE2K_ISR_RDC    0x40U
#define NE2K_ISR_PRX    0x01U
#define NE2K_DCR_BYTE_MODE 0x48U
#define NE2K_COMMAND_REMOTE_WRITE 0x12U
#define NE2K_COMMAND_REMOTE_READ  0x0aU
#define NE2K_COMMAND_TRANSMIT 0x26U
#define NE2K_TX_PAGE 0x40U
#define NE2K_ETHERNET_MIN_FRAME 60U
#define NE2K_ETHERNET_MAX_FRAME 1514U
#define NE2K_RX_PAGE_START 0x46U
#define NE2K_RX_PAGE_STOP  0x60U
#define NE2K_RX_HEADER_SIZE 4U
#define NE2K_RX_STATUS_OK 0x01U

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
    uint8_t mac_valid;
} ne2k_device_t;

/* Sonde le registre reset et prépare le mode arrêt/word pour une init ultérieure. */
int ne2k_probe(ne2k_device_t* device, uint16_t base_port, const ne2k_io_t* io);
/* Prépare les callbacks de ports i386 réels; retourne -1 hors noyau i386. */
int ne2k_i386_io(ne2k_io_t* io);
/* Initialise les paramètres invariants du contrôleur sans allocation. */
int ne2k_prepare(ne2k_device_t* device, const ne2k_io_t* io);
/* Configure un anneau RX et une page TX dans la mémoire locale du NE2000. */
int ne2k_configure_rings(ne2k_device_t* device, const ne2k_io_t* io);
/* Définit une MAC locale valide: non nulle et non multicast. */
int ne2k_set_mac(ne2k_device_t* device, const uint8_t mac[6]);
/* Lit les six octets pairs de la PROM NE2000 sans conserver de buffer externe. */
int ne2k_read_mac(ne2k_device_t* device, const ne2k_io_t* io);
/* Copie une trame caller-owned dans la RAM distante puis déclenche TX. */
int ne2k_tx_submit(ne2k_device_t* device, const ne2k_io_t* io,
                   const uint8_t* frame, uint16_t length);
/* Attache le périphérique à l’IRQ ISA fournie par le matériel, sans allocation. */
int ne2k_irq_attach(ne2k_device_t* device, const ne2k_io_t* io);
/* Acquitte l’ISR et compte les événements NE2000 observés par l’IRQ. */
void ne2k_irq_service(void);
uint32_t ne2k_irq_count(void);
/* Polling RX borné: lit une trame depuis la RAM distante vers un buffer appelant. */
int ne2k_rx_poll(ne2k_device_t* device, const ne2k_io_t* io,
                 uint8_t* frame, uint16_t frame_capacity,
                 uint16_t* frame_length);
/* Lit une trame puis décode son en-tête Ethernet et son ARP caller-owned. */
int ne2k_rx_poll_arp(ne2k_device_t* device, const ne2k_io_t* io,
                     uint8_t* frame, uint16_t frame_capacity,
                     uint16_t* frame_length,
                     net_ethernet_header_t* ethernet,
                     net_arp_packet_t* arp);
/* Extrait une trame reçue depuis un buffer DMA caller-owned vers la file RX. */
int ne2k_rx_extract(const uint8_t* dma_buffer, uint16_t dma_length,
                    net_nic_queue_t* rx_queue);

#endif
