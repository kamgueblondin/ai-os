# User stories AI-OS — backlog du prototype i386

**Date :** 17 août 2026  
**Source de vérité runtime :** [docs/ETAT_REEL.md](../docs/ETAT_REEL.md)  
**Périmètre :** hobby OS i386 Multiboot, QEMU, shell Ring 3 et IA locale — pas une distribution Linux. Ce document ne constitue pas le plan MOHHOS à long terme. Lexique : [docs/vocabulaire.md](../docs/vocabulaire.md).

La mention **fait** signifie que le comportement est observable dans le code et couvert par une commande de vérification. Les limites explicitement indiquées font partie du résultat : elles ne doivent pas être confondues avec des fonctionnalités livrées.

## Socle déjà livré

| ID | User story | État vérifié |
|---|---|---|
| AOS-001 | Démarrer un noyau Multiboot i386 | Boot QEMU, VGA/série, curseur bloc, historique Page Up/Down, `make run`, `make run-gui` et ISO GRUB BIOS |
| AOS-002 | Gérer mémoire physique, virtuelle et tas | PMM, VMM, heap, paging et `SYS_MEMINFO` |
| AOS-003 | Recevoir timer et clavier | PIC/PIT 100 Hz/i8042 ; préfixe `0xE0` (Page Up/Down, flèches) ; EOI IRQ0 avant le gestionnaire C |
| AOS-004 | Lire un initrd | Archive TAR (ustar) en lecture seule, `SYS_LISTDIR`, `SYS_READFILE` |
| AOS-005 | Exécuter un shell isolé | Shell ELF utilisateur et retour Ring 3 par `iret` |
| AOS-006 | Exposer une ABI de syscalls | ABI propre `int 0x80` ; aujourd’hui syscalls 0–89, `MAX_SYSCALLS = 90` |
| AOS-007 | Conserver de petits fichiers | Overlay ATA PIO persistant V2 et restauration V1/V2 |
| AOS-008 | Gérer plusieurs tâches | `spawn`, `yield`, `ps`, `kill`, plus préemption IRQ0 sûre |
| AOS-009 | Exécuter un ELF bloquant | `exec`, parent `TASK_WAITING`, réveil par `SYS_EXIT` |
| AOS-010 | Compléter localement avec GPT-2 | `SYS_GPT2_GENERATE`, GPT-2 124M optionnel, cache KV et SSE2 |
| AOS-011 | Tokeniser BPE GPT-2 | Vocabulaire/fusions BPE et décodage UTF-8 brut |
| AOS-012 | Prévenir les régressions | 299 tests C et contrats QEMU versionnés, dont `qemu-ne2k-status` |

## Tranche AOS-020 à AOS-025 — livrée

### AOS-020 — Compatibilité GGUF et réduction de latence

**En tant que** mainteneur du runtime local, **je veux** accepter de façon bornée la structure des checkpoints GGUF et préparer la quantification, **afin de** pouvoir évoluer au-delà du checkpoint FP32 initial.

**Livraison.** `gpt2_gguf.c` analyse GGUF v3, ses métadonnées, les descripteurs de tenseurs et leurs alignements. Le parseur a été validé contre un checkpoint GPT-2 réel contenant F32, Q3_K, Q4_K et Q6_K. `gpt2_quant.c` fournit FP16→FP32, le produit Q8_0×FP32 et les produits freestanding Q3_K/Q4_K/Q6_K × activation FP32, avec les tailles de super-blocs GGML (256 valeurs) et les contrôles de longueur associés. Le rapport GGUF expose désormais les compteurs de tenseurs K-quants supportés et `gpt2_gguf_find_tensor` recherche un descripteur borné par nom, forme, type, offset et taille calculée. Les tests Unity et les tests de robustesse couvrent les préfixes tronqués, compteurs excessifs, alignements invalides, comparaisons numériques synthétiques et recherche bornée de tenseurs Q4_K.

**Limite.** Les lots suivants ont ajouté un index caller-owned, un mapping de rôles et de couches, un chargement FAT16 et des briques de forward (attention, MLP, cache KV). Un checkpoint GGUF réel n’est pas encore sélectionnable pour `ai <texte>` : le chemin de génération livré au shell reste le checkpoint FP32 `llm.c v3`. Le cache KV et SSE2 améliorent ce chemin, mais le temps observé sous QEMU TCG sans KVM reste de l’ordre de 7 à 9 secondes pour une courte réponse, au-dessus de l’objectif inférieur à une seconde.

**Vérification.** `make test-all` (constat à la livraison : 257 tests ; chiffre courant dans [ETAT_REEL.md](../docs/ETAT_REEL.md)), puis `make clean && make all`. La conception et les références de layouts sont documentées dans [docs/aos020_gguf_quantization_design.md](../docs/aos020_gguf_quantization_design.md) et [docs/research_ggml_kquant_reference.md](../docs/research_ggml_kquant_reference.md).

### AOS-021 — BPE Unicode

**En tant qu’**utilisateur, **je veux** que les prompts non ASCII soient segmentés avec une logique de lettres Unicode utile, **afin de** ne pas réduire tous les textes internationaux à des octets opaques.

**Livraison.** Le tokenizer valide l’UTF-8 sur 2, 3 et 4 octets, rejette les formes overlong et les surrogates, puis reconnaît les lettres Latin étendu, Grec, Arabe, Hébreu, Devanagari, CJK et Hangul. Les emoji et symboles restent des séparateurs BPE, ce qui est testé.

**Limite.** Cette implémentation n’est pas une réimplémentation exhaustive de `\p{L}` Unicode.

### AOS-022 — Contrat d’intégration QEMU

**En tant que** contributeur, **je veux** une intégration QEMU versionnée, **afin de** vérifier un boot et un shell réels au-delà des tests C isolés.

**Livraison.** `tests/integration/test_qemu_core_contract.py` vérifie le boot, `ai-runtime`, l’overlay, la copie, `append` et le retour au shell. Son disque overlay de test est isolé de `build/overlay.img`, de sorte que le contrat est idempotent.

**Vérification.** `make integration-qemu`.

### AOS-023 — Stockage étendu

**En tant qu’**utilisateur, **je veux** des fichiers overlay plus nombreux et plus grands, **afin de** dépasser le snapshot de démonstration initial.

**Livraison.** Le format AIOV V2 porte l’overlay à 64 nœuds, 80 octets par chemin, 384 octets de données et 64 secteurs ATA. La restauration lit aussi les snapshots V1. Les tests couvrent une restauration V1 et un fichier V2 étendu.

**Limite.** L’overlay reste un petit snapshot AIOV persistant (64 nœuds, 384 octets). Le volume FAT16 lecture seule (AOS-026) est un support distinct à partir du LBA 64. Voir [docs/aos_fat_volume.md](../docs/aos_fat_volume.md).

### AOS-024 — Préemption IRQ0 sûre

**En tant qu’**utilisateur, **je veux** qu’une tâche Ring 3 qui ne coopère pas ne gèle pas le shell, **afin de** pouvoir garder une interaction clavier fiable.

**Livraison.** Le timer applique un quantum de 20 ticks seulement pour un cadre ayant `CS` et `SS` Ring 3 et lorsqu’une autre tâche utilisateur est `READY`. Cette garde interdit un `schedule()` depuis un cadre noyau incomplet. L’EOI IRQ0 reste placé avant le gestionnaire C.

**Vérification.** `make qemu-irq0-preemption` lance `spawn spin`, où `spin` ne fait aucun syscall, puis exige `echo irq0-preempt-ok` depuis le shell.

### AOS-025 — Réseau minimal et fournisseur OpenAI

**En tant qu’**utilisateur, **je veux** savoir exactement si le fournisseur en ligne peut émettre une requête, **afin de** ne pas croire qu’un profil sélectionné est déjà un client OpenAI fonctionnel.

**Livraison.** `ai-provider openai` est un stub explicite. `net-status` / `net-status json` publient la présence réelle d’une NIC NE2000 ISA (`SYS_NET_STATUS`). Sans carte, `nic=absent` ; avec `-device ne2k_isa`, `make qemu-ne2k-status` exige `nic=detected`. La commande `ai` avec ce profil n’émet aucune requête. ARP, IPv4, DHCP, DNS, TCP et TLS restent affichés absents : leurs codecs caller-owned existent (lots 113–154) mais aucune configuration live n’est raccordée au shell.

**Limite et suite.** Un client OpenAI réel exige encore un bail IPv4 live, DNS, un flux TCP utilisateur, un handshake TLS et HTTP. Les clés API doivent rester hors de l’initrd et du dépôt. QEMU peut relier `ne2k_isa` à un backend utilisateur [1] ; le guest a désormais un pilote, pas un client HTTPS.

**Vérification.** `make qemu-ai-provider` (OpenAI bloqué) et `make qemu-ne2k-status` (NIC détectée).

### AOS-026 — Volume FAT sur disque IDE (lot 68 livré)

**En tant qu’**utilisateur, **je veux** un volume FAT sur le disque IDE, **afin de** lire des fichiers plus grands que l’overlay AIOV sans adopter un système à inodes.

**Livraison lecture seule.** Le noyau monte un volume FAT16 préparé sur le disque IDE à partir du LBA 64, sans toucher aux 64 secteurs AIOV. `fat16-list` liste la racine 8.3 et `fat16-cat <8.3>` lit les fichiers chaînés par la FAT. Note : [docs/mohhos_foundation_increment_68_fat16_volume.md](../docs/mohhos_foundation_increment_68_fat16_volume.md).

**Critère de sortie.** Lire le BPB et la table d’allocation FAT16 (FAT12 acceptable), lister le répertoire racine 8.3, lire un fichier préparé sur l’image, sans toucher aux 64 secteurs AIOV. Écriture, noms longs et FAT32 sont hors de ce premier jalon. **ext2 n’est pas une option.**

## Tranche réseau AOS-113 à AOS-154 — primitives livrées

Les lots suivants sont **faits** au sens caller-owned / Unity / smoke NIC. Ils ne livrent pas un daemon DHCP, un socket utilisateur ni OpenAI. Détail : [docs/ETAT_REEL.md](../docs/ETAT_REEL.md) et les notes `docs/aos11*.md` … `docs/aos15*.md`.

| Lots | User story condensée | Vérification |
|---|---|---|
| AOS-113, AOS-132 | Diagnostic machine-lisible et bitmask `SYS_NET_STATUS` | `net-status json`, `make qemu-ne2k-status` |
| AOS-116 | Accès PCI borné pour une future NIC PCI | `test_pci` ; le boot reste ISA `0x300` |
| AOS-117–136 | Sonde NE2000, PROM MAC, anneaux, TX PIO, RX poll, IRQ3 | `test_ne2k`, boot `ne2k_boot_probe` |
| AOS-114, AOS-137–143 | Ethernet/ARP, réponse, cache, résolution active | `test_net_ethernet_arp` |
| AOS-121–123, AOS-140–146 | IPv4/UDP, DHCP Discover/Offer/Request/ACK, DNS A | `test_net_ipv4_udp`, `test_net_dhcp`, `test_net_dns` |
| AOS-124–125, AOS-147–154 | SYN, SYN-ACK, ACK, payload, séquence, retransmission bornée | `test_net_tcp` |
| AOS-126–128 | SHA-256, HMAC-SHA-256, framing TLS 1.2 record | `test_sha256`, `test_net_tls_record` |

**Limite commune.** Pas de bail automatique, pas de commande shell `dhcp`/`dns`/`tcp`, pas de timer RTO, pas de handshake TLS, pas de X.509, pas de HTTP, `openai=blocked`.

## Prochaines tranches, hors livraison actuelle

| Priorité | Sujet | Critère de sortie |
|---|---|---|
| 1 | Réseau live | Bail DHCP, DNS et flux TCP raccordés au guest, smoke QEMU d’injection, toujours sans secret intégré |
| 2 | TLS et HTTP | Handshake TLS, certificats, requête HTTP contrôlée, OpenAI optionnel hors image |
| 3 | Inférence GGUF | Génération réelle sur checkpoint GGUF et comparaison FP32/K-quants |
| 4 | Latence locale | Mesure sous matériel/KVM jusqu’à l’objectif cible |
| 5 | Écriture FAT | Création/écriture 8.3 sur le volume existant ; LFN/FAT32 hors périmètre |

La vision MOHHOS (microkernel, P2P, économie, multi-plateforme, etc.) reste une collection de spécifications dans `US/`. Elle ne doit pas être utilisée comme indicateur d’implémentation du prototype.

## Références

[1] [QEMU, *Network emulation*](https://www.qemu.org/docs/master/system/devices/net.html)
