# AI-OS — noyau pédagogique i386

[![Version](https://img.shields.io/badge/version-7-blue.svg)](https://github.com/kamgueblondin/ai-os)
[![Statut](https://img.shields.io/badge/statut-prototype-yellow.svg)](https://github.com/kamgueblondin/ai-os)
[![CI](https://github.com/kamgueblondin/ai-os/actions/workflows/ci.yml/badge.svg)](https://github.com/kamgueblondin/ai-os/actions/workflows/ci.yml)
[![Licence](https://img.shields.io/badge/licence-MIT-blue.svg)](LICENSE)

AI-OS est un **prototype de hobby OS i386 32-bit** démarrant par Multiboot. Ce n’est **pas** une distribution Linux, ni un clone Unix : noyau freestanding, ABI propre, pas de userland GNU. Il démarre sous QEMU, sépare Ring 0 et Ring 3, charge une archive initrd TAR et lance un shell ELF. Le noyau fournit des syscalls, un overlay AIOV persistant via ATA PIO, un volume FAT16 et des primitives FAT32 d’écriture/chaînage ainsi qu’un chemin d’inférence GPT-2 local optionnel. Le volume disque hors overlay est **FAT**, pas ext2.

> La source de vérité des fonctions réellement livrées est [docs/ETAT_REEL.md](docs/ETAT_REEL.md). Lexique : [docs/vocabulaire.md](docs/vocabulaire.md). La branche par défaut est `master`.

## Capacités vérifiées

| Domaine | Fonction réellement disponible |
|---|---|
| Démarrage | Multiboot BIOS, VGA/série, GDT, IDT, PIC, PIT, clavier PS/2, curseur bloc et historique d’écran (Page Up/Down) |
| Utilisateur | Shell ELF Ring 3, ABI de syscalls jusqu’à 110, `spawn`, `yield`, `exec`, `ps`, `kill` |
| Préemption | Quantum IRQ0 de 20 ticks, uniquement entre tâches utilisateur prêtes |
| IPC Foundation | Boîte aux lettres FIFO entre tâches Ring 3, 4 entrées par tâche, charge de 96 octets, `request_id` opaque, capacité client de 2 messages et instantané de file pour un propriétaire publié, événements best-effort ; pas de capabilities |
| VFS Foundation | `vfsserver` Ring 3, sources `vfs-info`/`vfs-mounts`/`vfs-stats`, quatre montages protégés (`initrd/`, `overlay/`, `fat16/`, `fat32/`) et quatre alias dynamiques au plus, lectures, métadonnées et listage source-spécifiques, écriture, suppression et renommage médiés pour l’overlay uniquement, compteurs volatils, transfert contrôlé de `vfs`, délégation backend révocable avec profils `read`/`mutate`/`full`, consultation unitaire et inventaire borné réservés au propriétaire publié courant |
| Découverte Foundation | Registre volatile de 8 services et 8 abonnements ; retrait, transfert par propriétaire, notifications et purge immédiate à la terminaison ; pas de capabilities |
| Fichiers | Archive initrd TAR en lecture seule, overlay AIOV V2 sur ATA PIO (64 nœuds, V1 compatible), transferts sectoriels x86 `rep insw`/`rep outsw`, volumes FAT16 et FAT32 accessibles en lecture, listage et statut via le VFS ; le médiateur `vfsserver` délègue `vfs-info` et le formatage de `vfs-stats` à `vfsvirtual` Ring 3 sur IPC borné, tandis que les mutations VFS restent limitées à l’overlay |
| IA locale | GPT-2 124M `llm.c v3`, BPE UTF-8, cache KV, SSE2 et top-k sur workspace statique borné, sans réseau au boot ni allocation dynamique dans le moteur d’inférence |
| GGUF | Runtime GPT-2 GGUF v3 local : catalogue FAT16 borné à 2 MiB, fenêtre de lecture caller-owned inter-clusters de 8 Kio et cache FAT isolé, embeddings Q3_K, décodage Q3_K sans branche, matrices Q3_K/Q4_K/Q6_K, bloc MLP réel à largeur 4C, cache KV statique, top-k en flux sans buffer de logits complet, session coopérative `ai` / `ai-continue` et shell `ai-model use gpt2.gguf`, sans allocation dynamique |
| Réseau | Pilote NE2000 ISA, `SYS_NET_STATUS`, codecs ARP/IPv4/UDP/DHCP/DNS/TCP/TLS record, renouvellement, réacquisition et backoff DHCP caller-owned différés hors IRQ0, conservation fournisseur et reprise HTTP/SSE contrôlées, reprise SSE fine `Last-Event-ID`, registre TCP statique et orchestrateur noyau transactionnel. `ai-acquire` puis `ai-tls-poll` valident DHCP/OFFER/REQUEST/ACK, ARP, DNS A, SYN, SYN-ACK, ClientHello, ServerHello minimal et ACK TCP sur un pair Ethernet QEMU local ; TLS authentifié, HTTP et OpenAI externes restent distincts et non validés. |

Les commandes du shell comprennent notamment `ls`, `cat`, `mkdir`, `rmdir`, `rm`, `cp`, `mv`, `write`, `append`, `touch`, `stat`, `grep`, `wc`, `sort`, `head`, `tail`, `fat16-list`, `fat16-cat`, `spawn`, `yield`, `ipc-send`, `ipc-recv`, `service-publish`, `service-grant`, `service-find`, `service-status <nom>`, `service-watch`, `vfs-backend-probe <fichier>`, `vfs-backend-write-probe <fichier> <texte>`, `vfs-backend-remove-probe <fichier>`, `vfs-backend-rename-probe <src> <dst>`, `vfs-grant <pid>`, `vfs-backend-grant <pid>`, `vfs-backend-grant-read <pid>`, `vfs-backend-grant-mutate <pid>`, `vfs-backend-revoke <pid>`, `vfs-backend-status <pid>`, `vfs-backend-list`, `vfs-read <chemin>`, `vfs-stat <chemin>`, `vfs-list <repertoire/>`, `vfs-list-page <repertoire/> <depart>`, `vfs-mkdir`, `vfs-rmdir`, `vfs-stats`, `vfs-mount-add <prefixe/> <initrd|overlay|fat16|fat32>`, `vfs-mount-remove <prefixe/>`, `vfs-write <chemin> <texte>`, `vfs-remove <chemin>`, `vfs-rename <src> <dst>`, `jobs`, `top`, `ai`, `ai-continue`, `ai-provider`, `ai-model`, `ai-runtime`, `ai-acquire`, `ai-tls-poll`, `ai-credential`, `net-status` et `net-status json`. La liste complète, y compris la supervision de tâches, est dans [docs/ETAT_REEL.md](docs/ETAT_REEL.md).
 `service-watch <nom>` abonne le shell à un service et `ipc-recv` affiche les transitions avec l’ancien PID, le nouveau PID et la raison ; la livraison est best-effort si la boîte IPC est pleine. Un processus qui possède un nom de service publié accepte au plus deux messages clients en attente : le troisième `ipc-send` retourne explicitement `ipc-send: capacite du service atteinte`, tandis qu’une tâche non publiée conserve les quatre entrées brutes. `service-status <nom>` affiche le PID propriétaire, la profondeur FIFO totale, la limite client et la capacité brute ; cet instantané public ne réserve rien et peut immédiatement devenir obsolète. `vfs-read` résout le service `vfs` au lieu d’accepter un PID ; le médiateur expose `vfs-read vfs-mounts`, sert `initrd/` depuis l’archive initrd exclusivement et `overlay/` depuis l’overlay ATA exclusivement. `vfs-mount-add assets/ initrd` ou `vfs-mount-add work/ overlay` ajoutent un alias local non recouvrant ; `vfs-mount-remove work/` le retire. La table contient huit entrées au plus, protège `initrd/`, `overlay/`, `fat16/` et `fat32/`, ne persiste pas et ne survit pas à un nouveau serveur VFS. Les alias overlay autorisent les mutations médiées existantes, tandis que les alias initrd, FAT16 et FAT32 restent en lecture seule. `vfs-stats` réutilise une lecture corrélée de la source virtuelle du même nom et affiche les compteurs 32 bits volatils `reads`, `writes`, `removes` et `renames`, y compris les requêtes refusées. `vfs-stat <chemin>` retourne via une requête corrélée la taille et le type de l’entrée depuis la source déclarée du montage, sans repli entre initrd et overlay ; l’instantané n’est ni atomique ni réservé. `vfs-list <repertoire/>` liste exclusivement la racine ou un sous-répertoire d’un montage déclaré, par exemple `initrd/bin/`. Le chemin doit être sûr, terminé par `/` et désigner un répertoire dans la source associée ; la réponse corrélée contient au plus quatre noms séparés par des sauts de ligne, dans une page de 80 octets. L’état `partiel` signale une page tronquée. `vfs-list-page <repertoire/> <depart>` renvoie un index suivant ou `end`, sans ordre contractuel, instantané atomique ni fusion initrd/overlay. Une requête d’écriture est bornée à 44 octets. `vfs-backend-status <pid>` transmet une demande corrélée à `vfsserver`, qui peut seul consulter le masque d’un bénéficiaire en tant que propriétaire public de `vfs`. La commande affiche `read`, `mutate` ou `full`; une capacité absente, révoquée ou un refus est explicitement signalé. Cette réponse est un instantané non atomique, sans réservation ni autorisation par chemin. `vfs-backend-list` expose au même propriétaire un inventaire corrélé de quatre couples PID/masque au plus ; une erreur retourne un inventaire vide et chaque entrée est encore soumise au contrôle backend au moment de son usage.
 Les programmes initrd incluent `shell`, `idle`, `spin`, `ipcserver`, `vfsserver`, `serviceclaim`, `vfsclaim`, `vfscapclaim`, `vfsreadclaim`, `vfsmutateclaim`, `waitchild`, `ok`, `fake_ai`, `ai_assistant`, `vfsvirtual` et `user_program`.

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
| `make all` | Noyau, initrd et image overlay IDE (AIOV + FAT16 à partir du LBA 64) |
| `make test-all` | Suite complète Unity/robustesse ; l’état courant validé est de 484 tests exécutés avec succès |
| `make qemu-smoke` | Scénarios QEMU classiques : overlay, persistance, spawn/yield et exec |
| `make gguf-disk` | Construit un disque FAT16 de déploiement contenant le modèle sous l’alias `GPT2.GGU` |
| `make qemu-gguf-smoke` | Démarre le disque GGUF, sélectionne `gpt2.gguf`, valide le premier token local réel puis `ai-continue` et affiche les deux latences |
| `make integration-qemu` | Contrats QEMU AOS-022, AOS-024, AOS-025, NE2000, IPC, VFS avec montages dynamiques, mutations médiées, révocation, notifications, cycle de vie et transfert Foundation |
| `make qemu-irq0-preemption` | Lance `spin` puis exige un shell toujours réactif |
| `make qemu-ai-provider` | Vérifie le diagnostic réseau et le blocage OpenAI |
| `make qemu-ne2k-status` | Vérifie `nic=detected` avec `-device ne2k_isa` |
| `make qemu-ne2k-acquire` | Valide `ai-acquire example.com` puis deux `ai-tls-poll` via DHCP, ARP, DNS A, SYN/SYN-ACK, ClientHello, ServerHello minimal et ACK contre un pair Ethernet socket local déterministe |
| `make qemu-ipc-foundation` | Lance `ipcserver`, envoie un message et vérifie sa réception |
| `make qemu-vfs-service` | Lance `vfsvirtual` puis `vfsserver`, vérifie la délégation Ring 3 de `vfs-info`, les volumes FAT16/FAT32 réels sur deux disques IDE, les alias dynamiques, la capacité, les refus, les mutations overlay, les profils backend, le transfert et la révocation |
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
├── gpt2_tokenizer.bin
└── gpt2-Q3_K_M.gguf       # optionnel : source de `make gguf-disk`
```

```bash
mkdir -p models
curl -L -o models/gpt2_124M.bin https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2_124M.bin
curl -L -o models/gpt2_tokenizer.bin https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2_tokenizer.bin
curl -L -o models/gpt2-124m-assets.sha256 https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2-124m-assets.sha256
(cd models && sha256sum -c gpt2-124m-assets.sha256)
make all
```

QEMU nécessite **1 Gio** de RAM quand le checkpoint FP32 est dans l’initrd. Dans le shell, utilisez `ai-provider local`, `ai-model list`, `ai-model use gpt2.gguf`, puis `ai bonjour` pour créer une session GGUF locale et `ai-continue` pour produire un jeton supplémentaire. `ai-runtime` et `rc` restent disponibles pour le diagnostic. Le profil GGUF garde 64 jetons de contexte dans son cache KV et retourne un token par appel pour éviter de monopoliser le noyau pendant un forward quantifié sur FAT16. Une reprise sans `ai <question>` préalable est refusée explicitement et un nouveau prompt GGUF remplace la session volatile en cours.

## GGUF, OpenAI et réseau : limites assumées

AI-OS valide la structure GGUF v3 et exécute désormais le profil GPT-2 GGUF quantifié depuis FAT16, y compris la projection descendante MLP à largeur `4C` et les lectures profondes de la chaîne FAT16. `make gguf-disk` conserve les poids hors initrd sous `GPT2.GGU`; le boot indexe seulement le catalogue et `ai-model use gpt2.gguf` sélectionne le syscall local dédié. Les lectures de QKV, MLP et logits sont séquentielles sur FAT16, le workspace reste entièrement statique et les transferts ATA PIO utilisent `rep insw`/`rep outsw`. Une fenêtre FAT16 caller-owned inter-clusters de 8 Kio regroupe jusqu’à seize secteurs ATA contigus, tandis qu’un cache FAT isolé évite que les transitions de chaîne n’évincent les poids anticipés. Dans une comparaison QEMU TCG, le premier token réel est passé de 48,89 s à 45,43 s et la continuation de 21,73 s à 20,83 s par rapport à la référence inter-clusters. Une répétition de référence à 49,04 s et 22,48 s confirme toutefois la variabilité de l’émulation ; ces mesures ne préjugent pas de la performance matérielle ni d’un gain reproductible.

Le profil `ai-provider openai` est activable de façon contrôlée : la session noyau relie DHCP, DNS, socket TCP statique, TLS, HTTP/SSE et extraction de réponse. `net-status` / `net-status json` publient la présence réelle d’une NIC NE2000 ISA ; l’appel externe reste désactivé tant qu’un bearer n’est pas configuré par `ai-credential`. Le contrat `make qemu-ne2k-acquire` prouve sur un pair local le bootstrap DHCP/DNS/ARP/SYN, la réception du SYN-ACK, l’émission du ClientHello, l’acceptation d’un ServerHello minimal et son ACK TCP ; il ne contacte aucun hôte Internet, ne valide pas de certificat, ne termine pas TLS et ne valide donc pas OpenAI. Les secrets OpenAI ne doivent jamais être inclus dans l’image, les logs série ou le dépôt.

## Tests et artefacts

`make test-all` a validé **486 tests exécutés avec succès** dans l’état courant, dont FAT16/FAT32, pagination native de leurs racines à travers le VFS, canal privé corrélé `vfsserver`/`vfsvirtual` pour `vfs-info` et `vfs-stats`, capacité backend VFS à renonciation autonome, routage IPC corrélé des réponses discordantes et dispatch de chemins externalisé dans une table d’opérations statique, curseur, saut, cache de secteur, fenêtre inter-clusters et cache FAT isolé, une lecture FAT16 profonde, projection GGUF top-k en flux, décodage Q3_K sans branche, session GGUF locale persistante et benchmark QEMU répétable de premier token/continuation, console VGA, PCI, SHA-256/HMAC, codecs Ethernet/ARP/IPv4/UDP/DHCP/DNS/TCP, NE2000, orchestrateur noyau LLM TLS/HTTP/SSE sur socket, GPT-2/GGUF, IPC, VFS, services, shell et RAMFS. `make qemu-ne2k-acquire` complète les smokes par l’observation déterministe de DHCP/OFFER/REQUEST/ACK, ARP, DNS A, SYN, SYN-ACK, ClientHello, ServerHello minimal et ACK TCP sur un segment socket QEMU isolé. `make qemu-gguf-smoke` valide séparément le disque GPT2.GGU, le premier token sélectionné dans le shell et `ai-continue`, avec deux durées mesurées. Ces tests QEMU ne préjugent pas de la performance matérielle ou d’une connectivité Internet publique. Les détails de périmètre et les limites restantes sont maintenus dans [docs/ETAT_REEL.md](docs/ETAT_REEL.md) et [docs/todo.md](docs/todo.md).

Une ISO BIOS/GRUB peut être produite avec l’initrd. Lorsque les poids GPT-2 sont fournis, ils sont bien incorporés à l’ISO pour un fonctionnement local sur une machine vierge ; ils restent ignorés par Git.

## Roadmap du prototype

Le backlog courant est [US/ai_os_us.md](US/ai_os_us.md). La vision MOHHOS est conservée séparément dans [US/README.md](US/README.md).

- [x] GPT-2 local, cache KV, SSE2 et top-k borné
- [x] Tokenizer BPE UTF-8 avec couverture de lettres Unicode ciblée
- [x] Sonde GGUF v3, kernels Q3_K/Q4_K/Q6_K, mapping de couches, forward Q3_K réel depuis FAT16 et session persistante `ai-continue`
- [x] Tests QEMU versionnés
- [x] Overlay ATA PIO V2, 64 nœuds et restauration V1
- [x] Préemption IRQ0 sûre entre tâches Ring 3
- [x] Stub OpenAI honnête et `net-status` dynamique (NIC absente ou NE2000 détectée)
- [x] Volume FAT16 lecture seule sur IDE (LBA 64), `fat16-list` / `fat16-cat`
- [x] Console VGA : curseur bloc et historique Page Up/Down
- [x] Pilote NE2000 ISA (sonde, anneaux, IRQ3, RX/TX PIO) et codecs ARP/IPv4/UDP/DHCP/DNS/TCP
- [x] SHA-256, HMAC-SHA-256 et framing TLS record (sans handshake)
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
- [x] Montages VFS dynamiques : quatre alias non recouvrants, volatils et corrélés au plus, sur `initrd`, `overlay`, `fat16` ou `fat32`
- [x] Capacité IPC de service : deux messages clients en attente au plus pour un propriétaire publié
- [x] État de capacité de service : instantané public PID/profondeur/limites via `service-status`
- [x] Métadonnées VFS médiées : taille et type source-spécifiques via `vfs-stat`
- [x] Listage VFS médié : racine ou sous-répertoire sûr, page de quatre noms au plus par source déclarée via `vfs-list <repertoire/>`
- [x] Création et suppression VFS de répertoire vide : `vfs-mkdir` et `vfs-rmdir` corrélés, limités aux montages overlay déclarés
- [x] Capacité backend VFS révocable : `vfs-backend-grant` délègue un accès backend sans céder le nom public `vfs`
- [x] Révocation backend VFS explicite : `vfs-backend-revoke` retire le droit d’un PID tout en préservant le propriétaire public `vfs`
- [x] Moindre privilège backend VFS : `vfs-backend-grant-read` autorise lecture, métadonnées et listage, mais interdit les mutations
- [x] Profil mutation seule VFS : `vfs-backend-grant-mutate` autorise les mutations backend mais interdit lecture, métadonnées et listage
- [x] Consultation médiée de capacité backend VFS : `vfs-backend-status` affiche le masque `read`, `mutate` ou `full` au propriétaire public de `vfs`
- [x] Inventaire médié de capacités backend VFS : `vfs-backend-list` affiche jusqu’à quatre délégations actives et leurs masques au propriétaire public de `vfs`
- [x] Capabilities backend VFS : délégation, moindre privilège, révocation indépendante, identité issue de la tâche Ring 3 et routage général des réponses discordantes
- [x] Externalisation locale du backend de chemins VFS : table statique d’opérations par source, droits de mutation explicites et alias dynamiques validés
- [~] Migration microkernel réelle : première délégation `vfsserver` → `vfsvirtual` Ring 3 livrée pour `vfs-info` et `vfs-stats` ; backend de stockage processus séparé et transferts de données hors du noyau encore ouverts
- [x] Latence GGUF quantifiée sur QEMU : benchmark répétable, rapport JSON de médiane/dispersion et référence premier token/continuation
- [ ] Optimisation supplémentaire de la latence GGUF sur une plateforme de référence stable, distincte de la variabilité QEMU TCG
- [x] Bootstrap DHCP/OFFER/REQUEST/ACK, ARP, DNS A, SYN/SYN-ACK, ClientHello, ServerHello minimal et ACK LLM observé sur NE2000 QEMU avec pair Ethernet local contrôlé
- [ ] Validation sur réseau physique/TAP ou bridge réel, handshake TLS authentifié, HTTP et client OpenAI effectif
- [x] Écriture FAT16 8.3, création de fichiers FAT32, écriture/chaînage FAT32, extension de la racine, LFN UTF-8 avec publication multi-entrée, reconstruction, renommage et suppression ; les deux volumes sont exposés au VFS en lecture seule — [docs/aos1989_1996_vfs_fat_dynamic_mounts.md](docs/aos1989_1996_vfs_fat_dynamic_mounts.md)

## Arborescence

```text
ai-os/
├── boot/                 # Multiboot et stubs ISR
├── kernel/               # mémoire, interruptions, tâches, syscalls et LLM
├── fs/                   # archive initrd TAR et overlay AIOV (ATA)
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
