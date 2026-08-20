# AOS-1373 à AOS-1384 — Réception HTTP et SSE LLM sur sockets TLS

## Objectif

Ce macro-lot poursuit la migration du client LLM vers l’API socket générique. Les réponses HTTPS et les flux SSE ne dépendent plus directement d’une structure `net_tcp_connection_t` détenue par l’ancien chemin NE2000 : elles prennent un identifiant socket, une session TLS et des buffers caller-owned.

## Contrat livré

| Primitive | Fonction |
|---|---|
| `net_socket_connection_snapshot` | Copie la connexion TCP d’un socket vers une structure caller-owned. |
| `net_socket_connection_restore` | Restaure cette connexion après un échec applicatif. |
| `net_llm_socket_open_response` | Déchiffre un record TLS socket et alimente l’accumulateur HTTP Content-Length. |
| `net_llm_socket_open_sse` | Déchiffre un record TLS socket et publie un delta SSE Ollama ou OpenAI. |

Les deux adaptateurs sauvegardent la connexion TCP, la session AES-GCM et l’accumulateur HTTP ou SSE avant réception. Un record non applicatif, un échec de déchiffrement ou une erreur du parseur HTTP/SSE restaure ces trois états et remet `consumed` à zéro. Une réception réussie conserve l’avancement de séquence TLS et le contenu publié.

> Aucun buffer global, allocation dynamique ou couplage NE2000 n’est introduit. Les buffers plaintext, HTTP, SSE et texte restent sous contrôle de l’appelant.

## Validation

Le test `test_llm_socket_opens_http_response` produit un record TLS côté serveur, l’encapsule dans TCP puis valide la réponse HTTP `200` et son body à travers le socket. `test_llm_socket_opens_openai_sse` exécute le même chemin avec une réponse HTTP chunked contenant un delta OpenAI `salut`. La compilation i386 réussit, le runner noyau passe **36/36 tests** et la suite complète passe **434/434 tests**.

## Limites restantes

Les adaptateurs de construction, transmission NE2000, réception TLS, HTTP et SSE sont désormais composables sur socket. Il reste à les appeler depuis un orchestrateur unique pilotant DNS, SYN, handshake TLS, émission, polling et retry, puis à raccorder ce chemin à la commande `ai` avec une configuration de fournisseur sûre. La planification périodique DHCP est également distincte et reste ouverte.

## Références

[1]: aos1353_1364_llm_socket_ne2k_bridge.md "Adaptateur LLM/socket et pont NE2000"
[2]: aos1365_1372_dhcp_live_renewal.md "Renouvellement DHCP live"
[3]: todo.md "Backlog AI-OS"

[1] [2] [3]
