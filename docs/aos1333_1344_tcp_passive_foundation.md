# AOS-1333 à AOS-1344 — fondation TCP d’écoute passive

## Objectif

Ce macro-lot ajoute la fondation déterministe de l’écoute TCP passive sans allocation dynamique. Une connexion caller-owned peut être placée dans l’état `LISTEN`, accepter un SYN entrant correspondant à son port local, produire un segment SYN-ACK, puis passer à `ESTABLISHED` après l’ACK final.

## Contrat livré

| Primitive | Contrat |
|---|---|
| `net_tcp_connection_listen` | Initialise une connexion avec port local, séquence initiale caller-owned et état `LISTEN`. |
| `net_tcp_connection_accept_syn` | Accepte uniquement un SYN entrant sans ACK, mémorise le quadruplet distant et passe à `SYN_RECEIVED`. |
| `net_tcp_connection_build_syn_ack` | Construit un segment SYN-ACK de 20 octets minimum avec les séquences validées. |
| `net_tcp_connection_accept_ack` | Accepte l’ACK final exact et passe de `SYN_RECEIVED` à `ESTABLISHED`. |

Les états `LISTEN` et `SYN_RECEIVED` sont ajoutés sans modifier la représentation existante de `net_tcp_connection_t`. Les contrôles rejettent les ports nuls, les ports de destination incohérents, les drapeaux SYN+ACK reçus au premier stade et les séquences ou acquittements incorrects.

## Limites

Ce lot ne publie pas encore de syscall `listen`/`accept`, ne réserve pas automatiquement un slot dans le registre `net_socket`, ne scrute pas la NIC NE2000 et n’émet pas encore le SYN-ACK sur le réseau. Le raccordement registre/socket/NIC constituera le prochain lot ; les buffers et l’état restent caller-owned.

## Validation

Le runner noyau passe **35/35 tests**. Les nouveaux tests vérifient la séquence `LISTEN → SYN_RECEIVED → ESTABLISHED`, le décodage du SYN-ACK généré et le rejet d’un SYN accompagné à tort d’un ACK. La suite complète doit être relancée avant la PR afin de confirmer l’absence de régression globale.

## Mémoire

Aucune allocation dynamique n’est introduite. Les buffers de segment sont fournis par l’appelant et les transitions modifient uniquement la structure de connexion passée en argument.

## Auteur

Manus AI

## Références

Aucune source externe n’est nécessaire : ce document décrit les contrats et tests présents dans le dépôt AI-OS.

[1]: ../kernel/net_tcp.h
[2]: ../kernel/net_tcp.c
[3]: ../tests/unit/kernel/test_net_tcp.c
