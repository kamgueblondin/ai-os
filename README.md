# AI-OS — noyau pédagogique i386

[![Version](https://img.shields.io/badge/version-7-blue.svg)](https://github.com/kamgueblondin/ai-os)
[![Statut](https://img.shields.io/badge/statut-prototype-yellow.svg)](https://github.com/kamgueblondin/ai-os)
[![CI](https://github.com/kamgueblondin/ai-os/actions/workflows/ci.yml/badge.svg)](https://github.com/kamgueblondin/ai-os/actions/workflows/ci.yml)
[![Licence](https://img.shields.io/badge/licence-MIT-blue.svg)](LICENSE)

AI-OS est un **prototype de hobby OS i386 32-bit** démarrant par Multiboot. Il démarre sous QEMU, sépare Ring 0 et Ring 3, charge un initrd TAR et lance un shell ELF. Le noyau fournit une ABI de syscalls, un overlay RAM persistant via ATA PIO et un chemin d’inférence GPT-2 local optionnel.

> La source de vérité des fonctions réellement livrées est [docs/ETAT_REEL.md](docs/ETAT_REEL.md). La branche par défaut est `master`.

## Capacités vérifiées

| Domaine | Fonction réellement disponible |
|---|---|
| Démarrage | Multiboot BIOS, VGA/série, GDT, IDT, PIC, PIT et clavier PS/2 |
| Utilisateur | Shell ELF Ring 3, syscalls 0–42, `spawn`, `yield`, `exec`, `ps`, `kill` |
| Préemption | Quantum IRQ0 de 20 ticks, uniquement entre tâches utilisateur prêtes |
| IPC Foundation | Boîte aux lettres FIFO entre tâches Ring 3, 4 entrées par tâche, charge de 96 octets, `request_id` opaque, capacité client de 2 messages et instantané de file pour un propriétaire publié, événements best-effort ; pas de capabilities |
| VFS Foundation | `vfsserver` Ring 3, sources `vfs-info`/`vfs-mounts`/`vfs-stats`, deux montages protégés et trois alias dynamiques initrd/overlay au plus, lectures, métadonnées et listage source-spécifiques de racine ou de sous-répertoire, écriture, suppression et renommage médiés, compteurs volatils, transfert contrôlé de `vfs` et backend réservé au propriétaire publié courant ; initrd/overlay encore noyau |
| Découverte Foundation | Registre volatile de 8 services et 8 abonnements ; retrait, transfert par propriétaire, notifications et purge immédiate à la terminaison ; pas de capabilities |
| Fichiers | Initrd TAR en lecture seule et overlay ATA PIO V2 persistant (64 nœuds, V1 compatible) |
| IA locale | GPT-2 124M `llm.c v3`, BPE UTF-8, cache KV, SSE2 et top-k, sans réseau au boot |
| GGUF | Sonde structurelle GGUF v3 et primitives Q8_0 ; pas encore d’inférence quantifiée |
| Réseau | `net-status` et profil OpenAI explicitement bloqué ; aucune pile réseau noyau |

Les commandes du shell comprennent notamment `ls`, `cat`, `mkdir`, `rmdir`, `rm`, `cp`, `mv`, `write`, `append`, `touch`, `stat`, `grep`, `wc`, `spawn`, `yield`, `ipc-send`, `ipc-recv`, `service-publish`, `service-grant`, `service-find`, `service-status <nom>`, `service-watch`, `vfs-backend-probe <fichier>`, `vfs-backend-write-probe <fichier> <texte>`, `vfs-backend-remove-probe <fichier>`, `vfs-backend-rename-probe <src> <dst>`, `vfs-grant <pid>`, `vfs-read <chemin>`, `vfs-stat <chemin>`, `vfs-list <repertoire/>`, `vfs-list-page <repertoire/> <depart>`, `vfs-stats`, `vfs-mount-add <prefixe/> <initrd|overlay>`, `vfs-mount-remove <prefixe/>`, `vfs-write <chemin> <texte>`, `vfs-remove <chemin>`, `vfs-rename <src> <dst>`, `jobs`, `top`, `ai`, `ai-provider`, `ai-model`, `ai-runtime` et `net-status`.
 `service-watch <nom>` abonne le shell à un service et `ipc-recv` affiche les transitions avec l’ancien PID, le nouveau PID et la raison ; la livraison est best-effort si la boîte IPC est pleine. Un processus qui possède un nom de service publié accepte au plus deux messages clients en attente : le troisième `ipc-send` retourne explicitement `ipc-send: capacite du service atteinte`, tandis qu’une tâche non publiée conserve les quatre entrées brutes. `service-status <nom>` affiche le PID propriétaire, la profondeur FIFO totale, la limite client et la capacité brute ; cet instantané public ne réserve rien et peut immédiatement devenir obsolète. `vfs-read` résout le service `vfs` au lieu d’accepter un PID ; le médiateur expose `vfs-read vfs-mounts`, sert `initrd/` depuis l’archive initrd exclusivement et `overlay/` depuis l’overlay ATA exclusivement. `vfs-mount-add assets/ initrd` ou `vfs-mount-add work/ overlay` ajoutent un alias local non recouvrant ; `vfs-mount-remove work/` le retire. La table contient cinq entrées au plus, protège `initrd/` et `overlay/`, ne persiste pas et ne survit pas à un nouveau serveur VFS. Un alias overlay autorise les mutations médiées existantes, alors qu’un alias initrd reste en lecture seule. `vfs-stats` réutilise une lecture corrélée de la source virtuelle du même nom et affiche les compteurs 32 bits volatils `reads`, `writes`, `removes` et `renames`, y compris les requêtes refusées. `vfs-stat <chemin>` retourne via une requête corrélée la taille et le type de l’entrée depuis la source déclarée du montage, sans repli entre initrd et overlay ; l’instantané n’est ni atomique ni réservé. `vfs-list <repertoire/>` liste exclusivement la racine ou un sous-répertoire d’un montage déclaré, par exemple `initrd/bin/`. Le chemin doit être sûr, terminé par `/` et désigner un répertoire dans la source associée ; la réponse corrélée contient au plus quatre noms séparés par des sauts de ligne, dans une page de 80 octets. L’état `partiel` signale une page tronquée. `vfs-list-page <repertoire/> <depart>` renvoie un index suivant ou `end`, sans ordre contractuel, instantané atomique ni fusion initrd/overlay. Une requête d’écriture est bornée à 44 octets.
 Les programmes initrd incluent `shell`, `idle`, `spin`, `ipcserver`, `vfsserver`, `serviceclaim`, `vfsclaim`, `ok`, `fake_ai`, `ai_assistant` et `user_program`.

## Démarrage rapide

Sur Debian ou Ubuntu, installez les dépendances puis construisez le noyau et l’initrd.

```bash
git clone https://github.com/kamgueblondin/ai-os.git
cd ai-os
make deps
make all
make test-all
make integration-qemu
make run
```

| Cible | Rôle |
|---|---|
| `make all` | Noyau, initrd et image overlay IDE de 64 secteurs |
| `make test-all` | 207 tests C Unity/robustesse sans dépendre des poids GPT-2 |
| `make qemu-smoke` | Scénarios QEMU classiques : overlay, persistance, spawn/yield et exec |
| `make integration-qemu` | Contrats QEMU AOS-022, AOS-024, AOS-025, IPC, VFS avec montages dynamiques, mutations médiées, révocation, notifications, cycle de vie et transfert Foundation |
| `make qemu-irq0-preemption` | Lance `spin` puis exige un shell toujours réactif |
| `make qemu-ai-provider` | Vérifie le diagnostic réseau et le blocage OpenAI |
| `make qemu-ipc-foundation` | Lance `ipcserver`, envoie un message et vérifie sa réception |
| `make qemu-vfs-service` | Lance `vfsserver`, vérifie listage source-spécifique de racine et sous-répertoire, alias dynamiques initrd/overlay, capacité, refus, mutations corrélées, transfert et révocation |
| `make qemu-service-grant` | Publie `demo`, observe l’événement de transfert et de purge, puis vérifie son nettoyage |
| `make iso` | Produit l’ISO BIOS/GRUB bootable |
| `make run` / `make run-gui` | Session QEMU interactive curses ou GTK |

Pour construire l’ISO, installez également GRUB et xorriso.

```bash
sudo apt-get install -y grub-pc-bin xorriso
make iso
make run-iso
```

## GPT-2 local, sans réseau au démarrage

Les poids ne sont **pas** dans Git. Les actifs validés sont distribués par la [release `gpt2-124m-assets`](https://github.com/kamgueblondin/ai-os/releases/tag/gpt2-124m-assets).

```text
models/
├── gpt2_124M.bin
└── gpt2_tokenizer.bin
```

```bash
mkdir -p models
curl -L -o models/gpt2_124M.bin https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2_124M.bin
curl -L -o models/gpt2_tokenizer.bin https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2_tokenizer.bin
curl -L -o models/gpt2-124m-assets.sha256 https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2-124m-assets.sha256
(cd models && sha256sum -c gpt2-124m-assets.sha256)
make all
```

QEMU nécessite **1 Gio** de RAM quand ce checkpoint est dans l’initrd. Dans le shell, utilisez `ai hello`, `ai-provider local`, `ai-model list`, `ai-runtime` et `rc`. La génération est volontairement bornée à 64 jetons de contexte et quatre jetons nouveaux. Sous QEMU TCG sans KVM, le temps d’une courte génération reste de l’ordre de **7 à 9 secondes** : l’objectif inférieur à une seconde n’est pas atteint.

## GGUF, OpenAI et réseau : limites assumées

AI-OS valide la structure de fichiers GGUF v3 et fournit des primitives Q8_0×FP32, mais les kernels Q3_K/Q4_K/Q6_K requis pour exécuter les checkpoints GPT-2 quantifiés ne sont pas encore présents. `ai-model` peut mémoriser un profil `.gguf`, sans le faire exécuter.

Le profil `ai-provider openai` est un **stub contrôlé**. `net-status` affiche l’absence de pilote Ethernet, ARP, IPv4, DHCP, DNS, TCP et TLS ; une commande `ai` avec ce profil ne transmet aucune requête et annonce l’indisponibilité. QEMU peut connecter une carte ISA ou PCI émulée à un backend hôte, mais cette possibilité ne remplace pas un pilote ni une pile protocolaire dans le noyau invité [1]. Les secrets OpenAI ne doivent jamais être inclus dans l’image, les logs série ou le dépôt.

## Tests et artefacts

`make test-all` a validé **207/207** tests : PMM (17), syscall (48), tâches (21), overlay (8), tokenizer (15), GGUF (5), quantification (5), échantillonnage GPT-2 (4), IPC (6), file IPC différée (4), protocole VFS (22), registre de services (14), shell (25), RAMFS (10) et robustesse GGUF (3). `make integration-qemu` ajoute six validations QEMU séparées, dont les contrats IPC, capacité et instantanés de profondeur d’un propriétaire de service publié, médiateur VFS corrélé avec conservation locale, alias dynamiques initrd/overlay, capacité et protection de la table, métadonnées et listage source-spécifiques de racine ou de sous-répertoire, écriture, suppression et renommage médiés, sources virtuelles, compteurs VFS, transfert, révocation et notifications de service ; il réinitialise son disque de contrat sans toucher à `build/overlay.img`. Les délais de frappe QEMU sont stabilisés à 0,65 s pour le smoke cœur, 0,80 s pour les extras, 0,90 s pour le contrat VFS et 0,55 s pour le contrat de service ; ces contrats relancent au plus trois commandes jusqu’à leur marqueur fonctionnel attendu, notamment pour les chemins IPC, VFS et service sensibles aux doubles frappes PS/2.

Une ISO BIOS/GRUB peut être produite avec l’initrd. Lorsque les poids GPT-2 sont fournis, ils sont bien incorporés à l’ISO pour un fonctionnement local sur une machine vierge ; ils restent ignorés par Git.

## Roadmap du prototype

Le backlog courant est [US/ai_os_us.md](US/ai_os_us.md). La vision MOHHOS est conservée séparément dans [US/README.md](US/README.md).

- [x] GPT-2 local, cache KV, SSE2 et top-k borné
- [x] Tokenizer BPE UTF-8 avec couverture de lettres Unicode ciblée
- [x] Sonde GGUF v3 et primitives Q8_0
- [x] Tests QEMU versionnés
- [x] Overlay ATA PIO V2, 64 nœuds et restauration V1
- [x] Préemption IRQ0 sûre entre tâches Ring 3
- [x] Stub OpenAI/network honnête et testable
- [x] IPC Foundation non bloquant entre tâches Ring 3, avec identité d’émetteur noyau
- [x] Médiateur VFS Ring 3, lecture bornée et réponse IPC structurée
- [x] Registre de services nommé et cycle de vie : retrait propriétaire, nettoyage sur `exit`/`kill`
- [x] Corrélation requête-réponse locale : `request_id` IPC, réponse VFS filtrée et contrat QEMU
- [x] Transfert limité de publication : propriétaire, bénéficiaire Ring 3 et nettoyage après `kill`
- [x] Conservation locale bornée des messages IPC non corrélés pendant `vfs-read`
- [x] Première source virtuelle VFS (`vfs-info`) construite par `vfsserver` Ring 3
- [x] Voie backend VFS réservée au propriétaire publié de `vfs`
- [x] Transfert de `vfs` vérifié : révocation de l’ancien propriétaire et purge du nouveau
- [x] Politique de montage VFS bornée : `initrd/` déclaré, sources virtuelles et refus hors préfixe
- [x] Notifications de service best-effort : abonnement borné, événements IPC de publication, transfert, retrait et purge
- [x] Écriture VFS médiée : montage `overlay/ rw`, requête IPC corrélée et backend réservé au propriétaire de `vfs`
- [x] Lectures VFS source-spécifiques : `initrd/` et `overlay/` ne partagent plus de repli backend implicite
- [x] Suppression VFS médiée : `vfs-remove overlay/<fichier>` est corrélé et réservé au propriétaire de `vfs`
- [x] Renommage VFS médié : source et destination `overlay/` sont corrélées et réservées au propriétaire de `vfs`
- [x] Statistiques VFS : compteurs volatils lecture-écriture-suppression-renommage exposés par `vfs-stats`
- [x] Montages VFS dynamiques : trois alias initrd/overlay non recouvrants, volatils et corrélés au plus
- [x] Capacité IPC de service : deux messages clients en attente au plus pour un propriétaire publié
- [x] État de capacité de service : instantané public PID/profondeur/limites via `service-status`
- [x] Métadonnées VFS médiées : taille et type source-spécifiques via `vfs-stat`
- [x] Listage VFS médié : racine ou sous-répertoire sûr, page de quatre noms au plus par source déclarée via `vfs-list <repertoire/>`
- [ ] Capabilities, révocation indépendante, identité vérifiée, routage général des réponses discordantes et externalisation d’un backend VFS
- [ ] Migration microkernel réelle
- [ ] Inference GGUF quantifiée et latence QEMU inférieure à une seconde
- [ ] Pilote NIC, DHCP, DNS, TCP, TLS et client OpenAI effectif
- [ ] Système de fichiers disque général (ext2/FAT)

## Arborescence

```text
ai-os/
├── boot/                 # Multiboot et stubs ISR
├── kernel/               # mémoire, interruptions, tâches, syscalls et LLM
├── fs/                   # initrd TAR et overlay ATA
├── userspace/            # shell et programmes Ring 3
├── tests/                # Unity, robustesse et contrats QEMU
├── models/               # actifs locaux ignorés par Git
├── docs/                 # état réel et guides
└── US/                   # backlog prototype et archives MOHHOS
```

## Contribution

```bash
make deps && make ci
make integration-qemu
```

Les contributions ne doivent pas inclure `build/`, les ELF utilisateurs, les images ISO ou les modèles. Le code et la documentation du prototype sont en français.

## Références

[1] [QEMU, *Network emulation*](https://www.qemu.org/docs/master/system/devices/net.html)
