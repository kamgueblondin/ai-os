#ifndef AIOS_NET_ETHERNET_ARP_H
#define AIOS_NET_ETHERNET_ARP_H

#include <stdint.h>

#define NET_ETHERNET_HEADER_SIZE 14U
#define NET_ARP_PACKET_SIZE 28U
#define NET_ETHERTYPE_IPV4 0x0800U
#define NET_ETHERTYPE_ARP  0x0806U
#define NET_ARP_HTYPE_ETHERNET 1U
#define NET_ARP_PTYPE_IPV4 0x0800U
#define NET_ARP_OPCODE_REQUEST 1U
#define NET_ARP_OPCODE_REPLY 2U

/* Aucune structure ne pointe dans une trame reçue: les adresses sont copiées. */
typedef struct {
    uint8_t destination[6];
    uint8_t source[6];
    uint16_t ethertype;
} net_ethernet_header_t;

typedef struct {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_size;
    uint8_t protocol_size;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint8_t sender_ipv4[4];
    uint8_t target_mac[6];
    uint8_t target_ipv4[4];
} net_arp_packet_t;

/* Parse exactement un en-tête Ethernet. Retourne 0 si la trame est trop courte. */
int net_ethernet_parse(const uint8_t* frame, uint32_t length,
                       net_ethernet_header_t* out);
/* Parse ARP Ethernet/IPv4 à partir du début d’une trame Ethernet. */
int net_arp_parse(const uint8_t* frame, uint32_t length,
                  net_arp_packet_t* out);
/* Construit une requête ARP Ethernet/IPv4 dans le buffer caller-owned. */
int net_arp_build_request(uint8_t* frame, uint32_t capacity,
                          const uint8_t sender_mac[6], const uint8_t sender_ipv4[4],
                          const uint8_t target_ipv4[4]);
/* Reconnaît uniquement une réponse ARP destinée à l’adresse IPv4 fournie. */
int net_arp_is_reply_for(const net_arp_packet_t* packet,
                         const uint8_t local_ipv4[4],
                         const uint8_t requested_ipv4[4]);

#endif
