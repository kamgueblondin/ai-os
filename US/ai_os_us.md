# User stories AI-OS — backlog du prototype i386

**Date :** 13 août 2026  
**Source de vérité runtime :** [docs/ETAT_REEL.md](../docs/ETAT_REEL.md)  
**Périmètre :** noyau i386 Multiboot, QEMU, shell Ring 3 et IA locale. Ce document ne constitue pas le plan MOHHOS à long terme.

La mention **fait** signifie que le comportement est observable dans le code et couvert par une commande de vérification. Les limites explicitement indiquées font partie du résultat : elles ne doivent pas être confondues avec des fonctionnalités livrées.

## Socle déjà livré

| ID | User story | État vérifié |
|---|---|---|
| AOS-001 | Démarrer un noyau Multiboot i386 | Boot QEMU, VGA/série, `make run`, `make run-gui` et ISO GRUB BIOS |
| AOS-002 | Gérer mémoire physique, virtuelle et tas | PMM, VMM, heap, paging et `SYS_MEMINFO` |
| AOS-003 | Recevoir timer et clavier | PIC/PIT 100 Hz/i8042 ; EOI IRQ0 envoyé avant le gestionnaire C |
| AOS-004 | Lire un initrd | TAR POSIX en lecture seule, `SYS_LISTDIR`, `SYS_READFILE` |
| AOS-005 | Exécuter un shell isolé | Shell ELF utilisateur et retour Ring 3 par `iret` |
| AOS-006 | Exposer une ABI de syscalls | Syscalls 0–22, `MAX_SYSCALLS = 23` |
| AOS-007 | Conserver de petits fichiers | Overlay ATA PIO persistant V2 et restauration V1/V2 |
| AOS-008 | Gérer plusieurs tâches | `spawn`, `yield`, `ps`, `kill`, plus préemption IRQ0 sûre |
| AOS-009 | Exécuter un ELF bloquant | `exec`, parent `TASK_WAITING`, réveil par `SYS_EXIT` |
| AOS-010 | Compléter localement avec GPT-2 | `SYS_GPT2_GENERATE`, GPT-2 124M optionnel, cache KV et SSE2 |
| AOS-011 | Tokeniser BPE GPT-2 | Vocabulaire/fusions BPE et décodage UTF-8 brut |
| AOS-012 | Prévenir les régressions | 161 tests C et contrats QEMU versionnés |

## Tranche AOS-020 à AOS-025 — livrée

### AOS-020 — Compatibilité GGUF et réduction de latence

**En tant que** mainteneur du runtime local, **je veux** accepter de façon bornée la structure des checkpoints GGUF et préparer la quantification, **afin de** pouvoir évoluer au-delà du checkpoint FP32 initial.

**Livraison.** `gpt2_gguf.c` analyse GGUF v3, ses métadonnées, les descripteurs de tenseurs et leurs alignements. Le parseur a été validé contre un checkpoint GPT-2 réel contenant F32, Q3_K, Q4_K et Q6_K. `gpt2_quant.c` fournit FP16→FP32 et le produit Q8_0×FP32 freestanding. Les tests Unity et les tests de robustesse couvrent les préfixes tronqués, compteurs excessifs et alignements invalides.

**Limite.** Les kernels Q3_K/Q4_K/Q6_K manquent : un modèle GGUF n’est pas encore exécutable. Le cache KV et SSE2 améliorent le chemin GPT-2 FP32, mais le temps observé sous QEMU TCG sans KVM reste de l’ordre de 7 à 9 secondes pour une courte réponse, au-dessus de l’objectif inférieur à une seconde.

**Vérification.** `make test-all`, puis `make integration-qemu`. La conception est documentée dans [docs/aos020_gguf_quantization_design.md](../docs/aos020_gguf_quantization_design.md).

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

**Limite.** L’overlay reste un petit cache persistant, pas un ext2/FAT ni un système de fichiers disque général.

### AOS-024 — Préemption IRQ0 sûre

**En tant qu’**utilisateur, **je veux** qu’une tâche Ring 3 qui ne coopère pas ne gèle pas le shell, **afin de** pouvoir garder une interaction clavier fiable.

**Livraison.** Le timer applique un quantum de 20 ticks seulement pour un cadre ayant `CS` et `SS` Ring 3 et lorsqu’une autre tâche utilisateur est `READY`. Cette garde interdit un `schedule()` depuis un cadre noyau incomplet. L’EOI IRQ0 reste placé avant le gestionnaire C.

**Vérification.** `make qemu-irq0-preemption` lance `spawn spin`, où `spin` ne fait aucun syscall, puis exige `echo irq0-preempt-ok` depuis le shell.

### AOS-025 — Réseau minimal et fournisseur OpenAI

**En tant qu’**utilisateur, **je veux** savoir exactement si le fournisseur en ligne peut émettre une requête, **afin de** ne pas croire qu’un profil sélectionné est déjà un client OpenAI fonctionnel.

**Livraison.** `ai-provider openai` est un stub explicite et `net-status` expose l’absence de pilote Ethernet, ARP, IPv4, DHCP, DNS, TCP et TLS. La commande `ai` avec ce profil n’émet aucune requête et informe l’utilisateur que le transport bare-metal est absent. Le smoke QEMU `make qemu-ai-provider` garantit ce comportement.

**Limite et suite.** Aucun NIC ni pile réseau n’est fourni. Un client OpenAI réel exige, dans l’ordre, un pilote NIC, Ethernet, ARP, IPv4, DHCP/UDP, DNS, TCP, TLS et HTTP. Les clés API doivent rester hors de l’initrd et du dépôt. La documentation QEMU confirme que l’émulateur peut relier une NIC ISA/PCI à un backend, mais cette fonction hôte ne remplace pas une pile dans le guest [1].

## Prochaines tranches, hors livraison actuelle

| Priorité | Sujet | Critère de sortie |
|---|---|---|
| 1 | Inference GGUF quantifiée | Kernels Q3_K/Q4_K/Q6_K, comparaison numérique et génération GPT-2 réelle |
| 2 | Latence locale | Mesure sous matériel/KVM et optimisation documentée jusqu’à l’objectif cible |
| 3 | Réseau effectif | NIC, DHCP, DNS, TCP/TLS et requête HTTP contrôlée, sans secret intégré |
| 4 | Système de fichiers général | Driver de blocs et ext2 ou FAT avec tests de reprise |

La vision MOHHOS (microkernel, P2P, économie, multi-plateforme, etc.) reste une collection de spécifications dans `US/`. Elle ne doit pas être utilisée comme indicateur d’implémentation du prototype.

## Références

[1] [QEMU, *Network emulation*](https://www.qemu.org/docs/master/system/devices/net.html)
