# État réel d’AI-OS

**Date de constat :** 13 août 2026

**Référence :** prototype pédagogique i386 32-bit, BIOS/Multiboot, QEMU et Ring 3.

**Rôle :** cette page décrit uniquement les fonctions observables dans le code et les tests. Elle prévaut sur les diagnostics historiques et sur la vision MOHHOS.

AI-OS démarre sans système hôte dans QEMU, charge un initrd TAR, lance un shell ELF en Ring 3 et peut exécuter localement GPT-2 124M si les deux actifs binaires sont intégrés à l’image. Il demeure un **prototype de noyau**, non un système d’exploitation généraliste.

| Périmètre | État vérifié |
|---|---|
| Démarrage et espace utilisateur | Noyau Multiboot i386, VGA/série, clavier PS/2, shell ELF Ring 3 |
| IA locale | GPT-2 124M `llm.c v3`, BPE, cache KV, SSE2 et top-k, sans réseau au boot |
| Stockage | Initrd TAR en lecture seule et overlay ATA PIO persistant V2 |
| Ordonnancement | Coopératif par syscall et quantum IRQ0 sûr entre tâches utilisateur |
| Réseau | **Aucun transport réseau noyau** ; diagnostic et profil OpenAI explicitement bloqués |

> Les poids GPT-2 ne sont pas versionnés dans Git. Une image utilisable sans réseau les embarque dans l’initrd au moment du build.

## Fonctions livrées

### Noyau, stockage et tâches

Le noyau configure GDT, IDT, PIC 8259, PIT 100 Hz, i8042 PS/2, PMM/VMM/heap, paging, chargement ELF 32-bit, syscalls et initrd TAR. L’EOI de l’IRQ0 est envoyé **avant** le gestionnaire C : lorsqu’un changement de contexte effectue un `iret` sans revenir dans le stub, IRQ1 clavier n’est donc pas laissée bloquée.

L’overlay RAM persistant utilise désormais le format snapshot **V2** : 64 nœuds, chemins de 80 octets, contenu de 384 octets et 64 secteurs ATA PIO. `overlay_restore()` reconnaît également le format V1 afin de restaurer les images de disque déjà créées. Il ne s’agit pas d’un système de fichiers général : ext2, FAT, répertoires sur disque et cache de blocs sont absents.

Les appels `spawn`, `yield` et `exec` continuent de changer de contexte depuis le cadre utilisateur de `int 0x80`. En complément, IRQ0 déclenche un round-robin toutes les 20 interruptions uniquement si le cadre interrompu est Ring 3 et si une **autre** tâche utilisateur est `READY`. Cela évite les cadres noyau incomplets et empêche un quantum inutile en mono-tâche. Le contrat QEMU lance `spin`, une boucle utilisateur sans syscall, puis exige une nouvelle commande du shell : la réactivité obtenue démontre la préemption réelle.

### IA locale, GGUF et BPE

Le chemin `ai <texte>` appelle `SYS_GPT2_GENERATE` avec le profil local GPT-2. Le chargeur utilise le checkpoint `llm.c v3`, le tokenizer binaire, des activations CPU freestanding, le cache clé/valeur par couche et position, SSE2 et un échantillonnage top-k. Le contexte est limité à 64 jetons ; l’interface indique quatre jetons générés au maximum afin de borner l’exécution. Sous QEMU TCG sans KVM, l’objectif inférieur à une seconde n’est **pas atteint** : les mesures disponibles restent de l’ordre de 7 à 9 secondes pour une courte génération. L’amélioration du cache KV et de SSE2 reste néanmoins substantielle par rapport à l’ancien chemin non optimisé.

AOS-020 ajoute une sonde freestanding **GGUF v3** bornée. Elle valide magic, version, métadonnées, tenseurs et alignement ; elle a été exercée contre un checkpoint GPT-2 réel contenant des tenseurs F32, Q3_K, Q4_K et Q6_K. Les primitives FP16→FP32 et Q8_0×FP32 sont présentes. L’exécution de checkpoints Q3_K/Q4_K/Q6_K ne l’est pas encore, faute des kernels correspondants ; GGUF est donc une validation structurelle, pas un backend d’inférence quantifié.

Le tokenizer BPE gère maintenant un décodage UTF-8 validé et une classe de lettres couvrant notamment Latin étendu, Grec, Arabe, Hébreu, Devanagari, CJK et Hangul. Les emoji et symboles restent des séparateurs BPE. Cette couverture est volontairement pratique et bornée ; elle n’implémente pas l’intégralité de `\p{L}` Unicode.

### Réseau et fournisseur OpenAI — AOS-025

La commande `ai-provider openai` ne réalise **aucun** appel réseau. Elle ne fait que sélectionner un profil, puis `ai` répond explicitement que le transport OpenAI est indisponible. La commande `net-status` expose le contrat réel : aucun pilote Ethernet, ARP, IPv4, DHCP, DNS, TCP ou TLS n’est initialisé ; aucune requête ni secret n’est envoyé depuis l’image.

QEMU sait relier une carte réseau virtuelle ISA ou PCI à un backend hôte, et son backend utilisateur peut fournir DHCP sans privilèges. Cette capacité de l’émulateur ne crée toutefois ni pilote de carte ni pile réseau dans le guest [1]. Une suite réseau réaliste devra livrer, dans cet ordre, un pilote NIC, la réception/transmission Ethernet, ARP, IPv4, UDP/DHCP, DNS, TCP, TLS et seulement ensuite un client HTTP compatible OpenAI. Les clés API doivent rester hors de l’initrd, du dépôt et des logs série.

| Jalons AOS | Livraison réellement vérifiée |
|---|---|
| AOS-020 | Sonde GGUF v3, tests de bornes et primitives Q8_0 ; exécution quantifiée non livrée |
| AOS-021 | BPE UTF-8 avec catégories de lettres Unicode ciblées |
| AOS-022 | Contrat QEMU versionné : boot, runtime IA, overlay, append et retour au shell |
| AOS-023 | Snapshot ATA overlay V2 avec restauration V1/V2 |
| AOS-024 | Préemption IRQ0 Ring 3, quantum 20 ticks, contrat `spawn spin` puis shell réactif |
| AOS-025 | Stub OpenAI honnête, `net-status` et smoke QEMU ; aucune pile réseau fournie |

## Shell et ABI observables

Le shell Ring 3 propose `ls`, `cat`, `mkdir`, `rmdir`, `rm`, `cp`, `mv`, `write`, `append`, `touch`, `stat`, `test`, `grep`, `wc`, `sort`, `head`, `tail`, `ps`, `jobs`, `top`, `spawn`, `yield`, `kill`, `exec`, `mem`, `uptime`, `history`, `alias`, `env`, `ai`, `ai-provider`, `ai-model`, `ai-runtime` et `net-status`. Les opérations de fichiers modifiables passent par l’overlay noyau ; l’initrd demeure en lecture seule. Les programmes empaquetés incluent `shell`, `idle`, `spin`, `ok`, `fake_ai`, `ai_assistant` et `user_program`.

L’ABI contient les syscalls 0–22 (`MAX_SYSCALLS = 23`) dont `SYS_APPEND` et `SYS_GPT2_GENERATE`. Le profil local ne nécessite aucun secret ni aucune dépendance réseau à l’exécution.

## Vérification reproductible

```bash
sudo apt-get install -y build-essential gcc-multilib nasm qemu-system-i386 grub-pc-bin xorriso
make clean && make all       # noyau, initrd et disque overlay
make test-all                # 161/161 tests C Unity et robustesse
make integration-qemu        # contrats AOS-022, AOS-024 et AOS-025
make iso                     # ISO BIOS/GRUB bootable
make run                     # console curses
make run-gui                 # fenêtre GTK
```

La suite C exécute 161 assertions de tests : PMM (17), syscall (48), tâches (21), overlay (8), tokenizer (15), GGUF (5), quantification (5), échantillonnage GPT-2 (4), shell (25), RAMFS (10) et robustesse GGUF (3). `make integration-qemu` démarre trois machines QEMU séparées : le contrat cœur AOS-022 utilise une image overlay de test isolée, le contrat IRQ0 AOS-024 préempte `spin`, et le smoke AOS-025 vérifie que le profil OpenAI reste bloqué.

Le build a également produit une ISO GRUB BIOS avec l’initrd GPT-2 local. Avec les actifs disponibles dans l’environnement de vérification, l’ISO est d’environ 481 Mio ; les modèles restent exclus du versionnement.

## Absences importantes

AI-OS ne fournit pas de système de fichiers disque général, de pilote réseau, de pile TCP/IP/TLS, de client OpenAI/Ollama effectif, d’UEFI, de gestion multiprocesseur, de GUI native, de microkernel, d’IPC général ni des fonctionnalités de la vision MOHHOS. Les rapports historiques conservés dans `docs/` sont des éléments de chronologie et non la description de l’état courant.

## Références

[1] [QEMU, *Network emulation*](https://www.qemu.org/docs/master/system/devices/net.html)
