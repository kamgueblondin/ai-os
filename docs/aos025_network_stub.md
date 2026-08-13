# AOS-025 — Stub réseau bare-metal et profil OpenAI

**Statut :** livré comme **stub contrôlé**, pas comme pile réseau.

**Date :** 13 août 2026.

## Objet

Le shell historique possédait un profil `ai-provider openai`, mais le noyau ne disposait d’aucun transport pour réaliser une requête. Cette situation pouvait laisser croire qu’une sélection de fournisseur correspondait à une intégration OpenAI. Le jalon AOS-025 rend ce contrat explicite et testable.

| Élément | État dans cette livraison |
|---|---|
| `ai-provider openai` | Sélection de profil seulement |
| `ai <texte>` avec profil OpenAI | Refus explicite, aucune requête émise |
| `net-status` | Diagnostic en lecture seule des composants absents |
| Pilote Ethernet | Absent |
| ARP / IPv4 / DHCP | Absents |
| DNS / TCP / TLS / HTTP | Absents |
| Clé API dans l’image | Interdite |

> Le résultat attendu n’est pas une connexion simulée : l’utilisateur doit voir qu’aucune requête OpenAI ne peut partir du noyau actuel.

## Validation

Le contrat `make qemu-ai-provider` démarre l’image QEMU, vérifie le profil local, appelle `net-status`, sélectionne `openai`, exécute `ai hello` et exige le message d’indisponibilité. Il vérifie donc que le profil est visible sans produire un faux succès réseau.

Le contrat agrégé est :

```bash
make integration-qemu
```

Il comprend AOS-022 (boot et shell), AOS-024 (préemption IRQ0) et AOS-025 (profil fournisseur).

## Relation avec QEMU

QEMU peut émuler des cartes réseau ISA ou PCI et les relier à un backend hôte. Le backend utilisateur fournit notamment DHCP sans privilèges et réserve typiquement `10.0.2.2` au routeur virtuel, `10.0.2.3` au DNS et des baux à partir de `10.0.2.15` [1]. Cette fonction appartient à l’émulateur : elle ne donne pas au noyau invité un pilote NIC ou une pile TCP/IP.

Une expérimentation future pourra démarrer QEMU avec un contrôleur ISA compatible NE2000 et le backend utilisateur :

```bash
qemu-system-i386 \
  -kernel build/ai_os.bin -initrd my_initrd.tar -m 1024M \
  -netdev user,id=n0 -device ne2k_isa,netdev=n0
```

Cette ligne ne doit être utilisée comme test de transport qu’après ajout d’un pilote dans AI-OS ; aujourd’hui, le noyau ne l’initialise pas.

## Critères de sortie d’un client OpenAI effectif

Le passage du stub au client réseau demande une série de jalons séparés, chacun avec tests de bornes et intégration QEMU.

| Ordre | Composant | Critère de sortie |
|---|---|---|
| 1 | Pilote NIC | Initialisation, MAC, RX/TX et gestion d’interruptions ou polling |
| 2 | Ethernet/ARP | Encapsulation et résolution MAC vérifiées en QEMU |
| 3 | IPv4/UDP/DHCP | Bail DHCP et configuration de route sans configuration statique secrète |
| 4 | DNS/TCP | Résolution et flux TCP bornés, temporisations et nettoyage |
| 5 | TLS | Validation de certificat et stockage de confiance minimal documenté |
| 6 | HTTP OpenAI | Requête sortante explicitement autorisée, secret injecté hors image et effacé après usage |

Aucune clé API ne doit être committée, placée dans l’initrd, écrite dans l’overlay par défaut ou reproduite dans la sortie série. Une future interface de configuration devra demander le consentement de l’utilisateur avant toute requête externe.

## Références

[1] [QEMU, *Network emulation*](https://www.qemu.org/docs/master/system/devices/net.html)
