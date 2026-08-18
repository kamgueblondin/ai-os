#ifndef AIOS_NE2K_H
#define AIOS_NE2K_H

#include <stdint.h>
#include "net_nic.h"
#include "net_ethernet_arp.h"
#include "net_ipv4_udp.h"
#include "net_dhcp.h"
#include "net_dns.h"
#include "net_tcp.h"
#include "net_http_tls.h"
#include "rtc.h"

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
#define NE2K_LLM_PROVIDER_OLLAMA 0U
#define NE2K_LLM_PROVIDER_OPENAI 1U

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

#define NE2K_LLM_CONNECTION_IDLE 0U
#define NE2K_LLM_CONNECTION_SYN_SENT 1U
#define NE2K_LLM_CONNECTION_TLS_STARTED 2U
#define NE2K_LLM_CONNECTION_TLS_COMPLETE 3U
#define NE2K_LLM_CONNECTION_REQUEST_SENT 4U
typedef struct { uint8_t remote_ip[4]; uint8_t phase; } ne2k_llm_connection_state_t;

typedef struct {
    net_tcp_tls_stream_t stream;
    net_tls_handshake_t handshake;
    net_tls_transcript_t transcript;
    net_tls_x25519_context_t x25519;
    net_tls_aes_gcm_session_t session;
    uint8_t master_secret[48];
    uint8_t key_block[NET_TLS_AES_128_GCM_KEY_BLOCK_LENGTH];
    uint8_t peer_identity_validated;
    uint8_t complete;
} ne2k_tls_client_t;

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
/* Construit puis émet une trame Ethernet IPv4/UDP caller-owned. */
int ne2k_tx_udp(ne2k_device_t* device, const ne2k_io_t* io,
                uint8_t* frame, uint16_t frame_capacity,
                const uint8_t destination_mac[6],
                const uint8_t source_ipv4[4], const uint8_t destination_ipv4[4],
                uint16_t source_port, uint16_t destination_port,
                const uint8_t* payload, uint16_t payload_length);
/* Résout une IPv4 par ARP avec nombre d’essais borné et cache caller-owned. */
int ne2k_arp_resolve(ne2k_device_t* device, const ne2k_io_t* io,
                     net_arp_cache_t* cache,
                     uint8_t* request_frame, uint16_t request_capacity,
                     uint8_t* rx_frame, uint16_t rx_capacity,
                     const uint8_t local_mac[6], const uint8_t local_ipv4[4],
                     const uint8_t target_ipv4[4], uint16_t attempts);
/* Résout la MAC puis construit et émet un paquet IPv4/UDP. */
int ne2k_tx_udp_resolve(ne2k_device_t* device, const ne2k_io_t* io,
                        net_arp_cache_t* cache,
                        uint8_t* request_frame, uint16_t request_capacity,
                        uint8_t* rx_frame, uint16_t rx_capacity,
                        uint8_t* tx_frame, uint16_t tx_capacity,
                        const uint8_t local_ipv4[4], const uint8_t target_ipv4[4],
                        uint16_t source_port, uint16_t destination_port,
                        const uint8_t* payload, uint16_t payload_length,
                        uint16_t attempts);
/* Construit et diffuse un DHCP Discover dans un buffer Ethernet caller-owned. */
int ne2k_dhcp_discover(ne2k_device_t* device, const ne2k_io_t* io,
                       uint8_t* frame, uint16_t frame_capacity, uint32_t xid);
/* Polling RX borné d’une offre DHCP via UDP 67->68. */
int ne2k_dhcp_poll_offer(ne2k_device_t* device, const ne2k_io_t* io,
                         uint8_t* frame, uint16_t frame_capacity,
                         uint32_t expected_xid, uint16_t attempts,
                         net_dhcp_offer_t* offer);
int ne2k_dhcp_request(ne2k_device_t* device, const ne2k_io_t* io,
                      uint8_t* frame, uint16_t frame_capacity,
                      uint32_t xid, const uint8_t requested_ip[4],
                      const uint8_t server_ip[4]);
int ne2k_dns_query(ne2k_device_t* device, const ne2k_io_t* io,
                   net_arp_cache_t* cache, uint8_t* arp_request, uint16_t arp_request_capacity,
                   uint8_t* arp_rx, uint16_t arp_rx_capacity, uint8_t* frame, uint16_t frame_capacity,
                   const uint8_t local_ip[4], const uint8_t dns_ip[4], uint16_t id,
                   const char* hostname);
int ne2k_dns_poll_a(ne2k_device_t* device, const ne2k_io_t* io,
                    uint8_t* frame, uint16_t frame_capacity, uint16_t attempts,
                    uint16_t expected_id, net_dns_a_result_t* result);
int ne2k_tcp_syn(ne2k_device_t* device, const ne2k_io_t* io,
                 net_arp_cache_t* cache, uint8_t* arp_request, uint16_t arp_request_capacity,
                 uint8_t* arp_rx, uint16_t arp_rx_capacity, uint8_t* frame, uint16_t frame_capacity,
                 const uint8_t local_ip[4], const uint8_t remote_ip[4],
                 uint16_t local_port, uint16_t remote_port, uint32_t sequence);
/* Résout un hostname LLM par DNS A, résout son ARP puis émet SYN et publie la connexion/IPv4 caller-owned seulement au succès. */
int ne2k_llm_dns_syn_bootstrap(ne2k_device_t* device,const ne2k_io_t* io,net_arp_cache_t* cache,
                                uint8_t* arp_request,uint16_t arp_request_capacity,uint8_t* arp_rx,uint16_t arp_rx_capacity,
                                uint8_t* frame,uint16_t frame_capacity,const uint8_t local_ip[4],const uint8_t dns_ip[4],
                                uint16_t dns_id,const char* hostname,uint16_t dns_attempts,uint16_t arp_attempts,
                                uint16_t local_port,uint16_t remote_port,uint32_t local_sequence,
                                uint8_t remote_ip[4],net_tcp_connection_t* connection);
/* Contexte LLM caller-owned : IP résolue et phase de préconnexion, sans stockage de hostname ni de secret. */
int ne2k_llm_connection_state_init(ne2k_llm_connection_state_t* state);
/* Démarre DNS→ARP→SYN et publie la phase SYN_SENT uniquement après succès complet. */
int ne2k_llm_connection_start(ne2k_device_t* device,const ne2k_io_t* io,net_arp_cache_t* cache,
                              uint8_t* arp_request,uint16_t arp_request_capacity,uint8_t* arp_rx,uint16_t arp_rx_capacity,
                              uint8_t* frame,uint16_t frame_capacity,const uint8_t local_ip[4],const uint8_t dns_ip[4],
                              uint16_t dns_id,const char* hostname,uint16_t dns_attempts,uint16_t arp_attempts,
                              uint16_t local_port,uint16_t remote_port,uint32_t local_sequence,
                              ne2k_llm_connection_state_t* state,net_tcp_connection_t* connection);
/* Construit et émet le premier ACK TCP depuis une connexion caller-owned. */
int ne2k_tcp_ack(ne2k_device_t* device, const ne2k_io_t* io,
                 const net_arp_cache_t* cache, uint8_t* frame, uint16_t frame_capacity,
                 const uint8_t local_ip[4], const uint8_t remote_ip[4],
                 const net_tcp_connection_t* connection);
/* Construit et émet le FIN+ACK caller-owned de fermeture. */
int ne2k_tcp_fin(ne2k_device_t* device, const ne2k_io_t* io,
                 const net_arp_cache_t* cache, uint8_t* frame, uint16_t frame_capacity,
                 const uint8_t local_ip[4], const uint8_t remote_ip[4],
                 net_tcp_connection_t* connection);
/* Construit et émet un segment TCP ACK+payload caller-owned. */
int ne2k_tcp_data(ne2k_device_t* device, const ne2k_io_t* io,
                  const net_arp_cache_t* cache, uint8_t* frame, uint16_t frame_capacity,
                  const uint8_t local_ip[4], const uint8_t remote_ip[4],
                  const net_tcp_connection_t* connection, const uint8_t* payload,
                  uint16_t payload_length);
/* Retransmet le dernier payload caller-owned sans avancer le sequence. */
int ne2k_tcp_retransmit(ne2k_device_t* device, const ne2k_io_t* io,
                        const net_arp_cache_t* cache, uint8_t* frame, uint16_t frame_capacity,
                        const uint8_t local_ip[4], const uint8_t remote_ip[4],
                        net_tcp_connection_t* connection);
/* Extrait un segment TCP reçu vers un payload caller-owned borné. */
int ne2k_tcp_receive(const uint8_t* frame, uint16_t frame_length,
                     net_tcp_connection_t* connection, uint8_t* payload,
                     uint16_t payload_capacity, uint16_t* payload_length);
/* Polling et extraction TCP en une étape, tous les buffers appartenant à l’appelant. */
int ne2k_tcp_poll(ne2k_device_t* device, const ne2k_io_t* io,
                  uint8_t* frame, uint16_t frame_capacity,
                  net_tcp_connection_t* connection, uint8_t* payload,
                  uint16_t payload_capacity, uint16_t* payload_length);
/* Reçoit un payload TCP puis émet son ACK avec les adresses IP et le cache ARP fournis. */
int ne2k_tcp_poll_ack(ne2k_device_t* device, const ne2k_io_t* io,
                      const net_arp_cache_t* cache, uint8_t* rx_frame,
                      uint16_t rx_capacity, uint8_t* tx_frame, uint16_t tx_capacity,
                      const uint8_t local_ip[4], const uint8_t remote_ip[4],
                      net_tcp_connection_t* connection, uint8_t* payload,
                      uint16_t payload_capacity, uint16_t* payload_length);
/* Reçoit un FIN, passe l’état en CLOSE_WAIT/CLOSED et émet son ACK. */
int ne2k_tcp_poll_fin_ack(ne2k_device_t* device, const ne2k_io_t* io,
                          const net_arp_cache_t* cache, uint8_t* rx_frame,
                          uint16_t rx_capacity, uint8_t* tx_frame, uint16_t tx_capacity,
                          const uint8_t local_ip[4], const uint8_t remote_ip[4],
                          net_tcp_connection_t* connection);

/* Initialise l’état TLS intégralement caller-owned : stream, transcript, clés et session. */
int ne2k_tls_client_init(ne2k_tls_client_t* client,uint8_t* record_buffer,uint16_t record_capacity,
                         uint8_t* handshake_buffer,uint16_t handshake_capacity,
                         uint8_t* transcript_buffer,uint16_t transcript_capacity);
/* Consomme un retry de connexion puis réinitialise TCP/TLS en conservant les buffers caller-owned. */
int ne2k_tls_client_retry_reset(net_tcp_connection_t* connection,ne2k_tls_client_t* client,
                                net_tcp_connection_retry_t* retry,uint32_t local_sequence);
/* Construit, émet et confirme transactionnellement le ClientHello TLS sur TCP établi. */
int ne2k_tls_client_start(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                          uint8_t* tx_frame,uint16_t tx_capacity,const uint8_t local_ip[4],const uint8_t remote_ip[4],
                          net_tcp_connection_t* connection,ne2k_tls_client_t* client,
                          const uint8_t client_random[32],uint8_t* client_hello_record,uint32_t client_hello_capacity,
                          uint8_t retransmit_limit);
/* Accepte un SYN-ACK validé et émet ClientHello ; la connexion et le client ne sont publiés qu’après succès complet. */
int ne2k_tls_client_accept_syn_ack_start(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                                         uint8_t* tx_frame,uint16_t tx_capacity,const uint8_t local_ip[4],const uint8_t remote_ip[4],
                                         const net_tcp_view_t* syn_ack,net_tcp_connection_t* connection,ne2k_tls_client_t* client,
                                         const uint8_t client_random[32],uint8_t* client_hello_record,uint32_t client_hello_capacity,
                                         uint8_t retransmit_limit);
/* Polling NE2000 de SYN-ACK suivi automatiquement du ClientHello TLS. Retourne 1 si RX est vide. */
int ne2k_llm_syn_ack_tls_start(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                               uint8_t* rx_frame,uint16_t rx_capacity,uint8_t* tx_frame,uint16_t tx_capacity,
                               const uint8_t local_ip[4],const uint8_t remote_ip[4],net_tcp_connection_t* connection,
                               ne2k_tls_client_t* client,const uint8_t client_random[32],uint8_t* client_hello_record,
                               uint32_t client_hello_capacity,uint8_t retransmit_limit);
/* Polling de la phase SYN_SENT : démarre TLS après SYN-ACK et passe à TLS_STARTED seulement au succès. */
int ne2k_llm_connection_poll_tls_start(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                                       uint8_t* rx_frame,uint16_t rx_capacity,uint8_t* tx_frame,uint16_t tx_capacity,
                                       const uint8_t local_ip[4],ne2k_llm_connection_state_t* state,
                                       net_tcp_connection_t* connection,ne2k_tls_client_t* client,
                                       const uint8_t client_random[32],uint8_t* client_hello_record,
                                       uint32_t client_hello_capacity,uint8_t retransmit_limit);
/* Polling TLS authentifié via RTC ; la phase devient TLS_COMPLETE uniquement après Finished serveur valide. */
int ne2k_llm_connection_poll_tls(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                                 uint8_t* rx_frame,uint16_t rx_capacity,uint8_t* tx_frame,uint16_t tx_capacity,
                                 const uint8_t local_ip[4],ne2k_llm_connection_state_t* state,
                                 net_tcp_connection_t* connection,ne2k_tls_client_t* client,
                                 const uint8_t client_random[32],const uint8_t client_private[NET_TLS_X25519_KEY_LENGTH],
                                 const x509_certificate_view_t* trust_anchor,const char* hostname,const rtc_io_t* rtc_io,
                                 uint32_t* rsa_workspace,uint16_t rsa_workspace_length,uint32_t* x25519_workspace,
                                 uint16_t x25519_workspace_length,uint8_t* prf_workspace,uint32_t prf_workspace_capacity,
                                 uint8_t* tcp_segment,uint32_t tcp_segment_capacity,uint8_t* flight_records,
                                 uint32_t flight_records_capacity,uint32_t* flight_records_length,uint8_t* plaintext,
                                 uint16_t plaintext_capacity,uint8_t retransmit_limit,uint16_t* consumed);
/* Émet une requête LLM chiffrée et passe à REQUEST_SENT seulement après transmission réussie. */
int ne2k_llm_connection_request(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                                uint8_t* tx_frame,uint16_t tx_capacity,const uint8_t local_ip[4],
                                ne2k_llm_connection_state_t* state,net_tcp_connection_t* connection,
                                ne2k_tls_client_t* client,uint8_t provider,uint8_t* json,uint16_t json_capacity,
                                uint8_t* request,uint16_t request_capacity,const char* host,const char* path,
                                const char* bearer_token,const char* model,const uint8_t* prompt,uint16_t prompt_length,
                                uint8_t* tls_record,uint32_t tls_capacity,uint8_t retransmit_limit);
/* Polling NE2000 : authentifie les messages serveur, valide l’ancre/hostname, émet le flight X25519 puis traite le post-flight. */
int ne2k_tls_client_poll(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                         uint8_t* rx_frame,uint16_t rx_capacity,uint8_t* tx_frame,uint16_t tx_capacity,
                         const uint8_t local_ip[4],const uint8_t remote_ip[4],net_tcp_connection_t* connection,
                         ne2k_tls_client_t* client,const uint8_t client_random[32],const uint8_t client_private[NET_TLS_X25519_KEY_LENGTH],
                         const x509_certificate_view_t* trust_anchor,const char* hostname,const char* utc_time,
                         uint32_t* rsa_workspace,uint16_t rsa_workspace_length,
                         uint32_t* x25519_workspace,uint16_t x25519_workspace_length,
                         uint8_t* prf_workspace,uint32_t prf_workspace_capacity,
                         uint8_t* tcp_segment,uint32_t tcp_segment_capacity,
                         uint8_t* flight_records,uint32_t flight_records_capacity,uint32_t* flight_records_length,
                         uint8_t* plaintext,uint16_t plaintext_capacity,uint8_t retransmit_limit,uint16_t* consumed);
/* Variante du polling TLS qui valide leaf-intermédiaire-ancre avant le flight X25519. */
int ne2k_tls_client_poll_chain_two(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                                   uint8_t* rx_frame,uint16_t rx_capacity,uint8_t* tx_frame,uint16_t tx_capacity,
                                   const uint8_t local_ip[4],const uint8_t remote_ip[4],net_tcp_connection_t* connection,
                                   ne2k_tls_client_t* client,const uint8_t client_random[32],const uint8_t client_private[NET_TLS_X25519_KEY_LENGTH],
                                   const x509_certificate_view_t* intermediate,const x509_certificate_view_t* trust_anchor,const char* hostname,const char* utc_time,
                                   uint32_t* rsa_workspace,uint16_t rsa_workspace_length,
                                   uint32_t* x25519_workspace,uint16_t x25519_workspace_length,
                                   uint8_t* prf_workspace,uint32_t prf_workspace_capacity,
                                   uint8_t* tcp_segment,uint32_t tcp_segment_capacity,
                                   uint8_t* flight_records,uint32_t flight_records_capacity,uint32_t* flight_records_length,
                                   uint8_t* plaintext,uint16_t plaintext_capacity,uint8_t retransmit_limit,uint16_t* consumed);
/* Variante qui emploie automatiquement le premier intermédiaire reçu dans Certificate TLS. */
int ne2k_tls_client_poll_received_chain(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                                        uint8_t* rx_frame,uint16_t rx_capacity,uint8_t* tx_frame,uint16_t tx_capacity,
                                        const uint8_t local_ip[4],const uint8_t remote_ip[4],net_tcp_connection_t* connection,
                                        ne2k_tls_client_t* client,const uint8_t client_random[32],const uint8_t client_private[NET_TLS_X25519_KEY_LENGTH],
                                        const x509_certificate_view_t* trust_anchor,const char* hostname,const char* utc_time,
                                        uint32_t* rsa_workspace,uint16_t rsa_workspace_length,
                                        uint32_t* x25519_workspace,uint16_t x25519_workspace_length,
                                        uint8_t* prf_workspace,uint32_t prf_workspace_capacity,
                                        uint8_t* tcp_segment,uint32_t tcp_segment_capacity,
                                        uint8_t* flight_records,uint32_t flight_records_capacity,uint32_t* flight_records_length,
                                        uint8_t* plaintext,uint16_t plaintext_capacity,uint8_t retransmit_limit,uint16_t* consumed);
/* Variante de production : lit l’instant UTC stable depuis RTC avant la politique TLS à chaîne reçue. */
int ne2k_tls_client_poll_received_chain_rtc(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                                            uint8_t* rx_frame,uint16_t rx_capacity,uint8_t* tx_frame,uint16_t tx_capacity,
                                            const uint8_t local_ip[4],const uint8_t remote_ip[4],net_tcp_connection_t* connection,
                                            ne2k_tls_client_t* client,const uint8_t client_random[32],const uint8_t client_private[NET_TLS_X25519_KEY_LENGTH],
                                            const x509_certificate_view_t* trust_anchor,const char* hostname,const rtc_io_t* rtc_io,
                                            uint32_t* rsa_workspace,uint16_t rsa_workspace_length,
                                            uint32_t* x25519_workspace,uint16_t x25519_workspace_length,
                                            uint8_t* prf_workspace,uint32_t prf_workspace_capacity,
                                            uint8_t* tcp_segment,uint32_t tcp_segment_capacity,
                                            uint8_t* flight_records,uint32_t flight_records_capacity,uint32_t* flight_records_length,
                                            uint8_t* plaintext,uint16_t plaintext_capacity,uint8_t retransmit_limit,uint16_t* consumed);
/* Construit, chiffre et émet un POST JSON LLM uniquement après handshake TLS complet. */
int ne2k_https_llm_post_json(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                             uint8_t* tx_frame,uint16_t tx_capacity,const uint8_t local_ip[4],const uint8_t remote_ip[4],
                             net_tcp_connection_t* connection,ne2k_tls_client_t* client,
                             uint8_t* request,uint16_t request_capacity,const char* host,const char* path,
                             const uint8_t* json,uint16_t json_length,uint8_t* tls_record,uint32_t tls_capacity,
                             uint8_t retransmit_limit);
/* Polling d’une réponse HTTP Content-Length chiffrée après TLS complet ; retourne 1 tant que le body est incomplet. */
int ne2k_https_llm_poll_response(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                                  uint8_t* rx_frame,uint16_t rx_capacity,uint8_t* tx_frame,uint16_t tx_capacity,
                                  const uint8_t local_ip[4],const uint8_t remote_ip[4],net_tcp_connection_t* connection,
                                  ne2k_tls_client_t* client,uint8_t* plaintext,uint16_t plaintext_capacity,
                                  net_http_response_accumulator_t* accumulator,net_http_response_view_t* response,uint16_t* consumed);
/* Compose JSON, Bearer optionnel Ollama / obligatoire OpenAI, chiffre et émet une requête LLM après TLS complet. */
int ne2k_https_llm_request(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                           uint8_t* tx_frame,uint16_t tx_capacity,const uint8_t local_ip[4],const uint8_t remote_ip[4],
                           net_tcp_connection_t* connection,ne2k_tls_client_t* client,uint8_t provider,
                           uint8_t* json,uint16_t json_capacity,uint8_t* request,uint16_t request_capacity,
                           const char* host,const char* path,const char* bearer_token,const char* model,
                           const uint8_t* prompt,uint16_t prompt_length,uint8_t* tls_record,uint32_t tls_capacity,
                           uint8_t retransmit_limit);
/* Polling LLM : retourne 1 si le body HTTP est incomplet, 0 lorsque le texte provider est extrait. */
int ne2k_https_llm_poll_text(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                             uint8_t* rx_frame,uint16_t rx_capacity,uint8_t* tx_frame,uint16_t tx_capacity,
                             const uint8_t local_ip[4],const uint8_t remote_ip[4],net_tcp_connection_t* connection,
                             ne2k_tls_client_t* client,uint8_t provider,uint8_t* plaintext,uint16_t plaintext_capacity,
                             net_http_response_accumulator_t* accumulator,net_http_response_view_t* response,
                             uint8_t* text,uint16_t text_capacity,uint16_t* text_length,uint16_t* consumed);
/* Émet une requête LLM JSON avec `stream:true` après handshake TLS complet. */
int ne2k_https_llm_stream_request(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                                   uint8_t* tx_frame,uint16_t tx_capacity,const uint8_t local_ip[4],const uint8_t remote_ip[4],
                                   net_tcp_connection_t* connection,ne2k_tls_client_t* client,uint8_t provider,
                                   uint8_t* json,uint16_t json_capacity,uint8_t* request,uint16_t request_capacity,
                                   const char* host,const char* path,const char* bearer_token,const char* model,
                                   const uint8_t* prompt,uint16_t prompt_length,uint8_t* tls_record,uint32_t tls_capacity,
                                   uint8_t retransmit_limit);
/* Polling HTTPS d’un flux chunked/SSE ; retourne 1 sans delta, 0 après delta ou terminaison SSE. */
int ne2k_https_llm_poll_sse(ne2k_device_t* device,const ne2k_io_t* io,const net_arp_cache_t* cache,
                            uint8_t* rx_frame,uint16_t rx_capacity,uint8_t* tx_frame,uint16_t tx_capacity,
                            const uint8_t local_ip[4],const uint8_t remote_ip[4],net_tcp_connection_t* connection,
                            ne2k_tls_client_t* client,uint8_t provider,uint8_t* plaintext,uint16_t plaintext_capacity,
                            net_llm_sse_response_t* response,uint8_t* text,uint16_t text_capacity,
                            uint16_t* text_length,uint16_t* consumed);
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
/* Traite au plus une requête ARP locale et soumet sa réponse au TX caller-owned. */
int ne2k_arp_service(ne2k_device_t* device, const ne2k_io_t* io,
                     uint8_t* rx_frame, uint16_t rx_capacity,
                     uint8_t* tx_frame, uint16_t tx_capacity,
                     const uint8_t local_mac[6], const uint8_t local_ipv4[4]);
/* Lit une trame IPv4/UDP et expose une vue payload dans le buffer caller-owned. */
int ne2k_rx_poll_udp(ne2k_device_t* device, const ne2k_io_t* io,
                     uint8_t* frame, uint16_t frame_capacity,
                     uint16_t* frame_length, net_udp_view_t* udp);
/* Lit une trame IPv4/TCP et expose une vue TCP dans le buffer caller-owned. */
int ne2k_rx_poll_tcp(ne2k_device_t* device, const ne2k_io_t* io,
                     uint8_t* frame, uint16_t frame_capacity,
                     uint16_t* frame_length, net_tcp_view_t* tcp);
/* Extrait une trame reçue depuis un buffer DMA caller-owned vers la file RX. */
int ne2k_rx_extract(const uint8_t* dma_buffer, uint16_t dma_length,
                    net_nic_queue_t* rx_queue);

#endif
