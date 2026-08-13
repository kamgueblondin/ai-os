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
| IPC Foundation MOHHOS | Boîte aux lettres FIFO non bloquante entre tâches Ring 3, 4 entrées par tâche, charge de 96 octets et `request_id` corrélé |
| Découverte Foundation MOHHOS | Registre volatile de 8 services nommés ; retrait, transfert par propriétaire et nettoyage à la terminaison |
| Réseau | **Aucun transport réseau noyau** ; diagnostic et profil OpenAI explicitement bloqués |

> Les poids GPT-2 ne sont pas versionnés dans Git. Une image utilisable sans réseau les embarque dans l’initrd au moment du build.

## Fonctions livrées

### Noyau, stockage et tâches

Le noyau configure GDT, IDT, PIC 8259, PIT 100 Hz, i8042 PS/2, PMM/VMM/heap, paging, chargement ELF 32-bit, syscalls et initrd TAR. L’EOI de l’IRQ0 est envoyé **avant** le gestionnaire C : lorsqu’un changement de contexte effectue un `iret` sans revenir dans le stub, IRQ1 clavier n’est donc pas laissée bloquée.

L’overlay RAM persistant utilise désormais le format snapshot **V2** : 64 nœuds, chemins de 80 octets, contenu de 384 octets et 64 secteurs ATA PIO. `overlay_restore()` reconnaît également le format V1 afin de restaurer les images de disque déjà créées. Il ne s’agit pas d’un système de fichiers général : ext2, FAT, répertoires sur disque et cache de blocs sont absents.

Les appels `spawn`, `yield` et `exec` continuent de changer de contexte depuis le cadre utilisateur de `int 0x80`. En complément, IRQ0 déclenche un round-robin toutes les 20 interruptions uniquement si le cadre interrompu est Ring 3 et si une **autre** tâche utilisateur est `READY`. Cela évite les cadres noyau incomplets et empêche un quantum inutile en mono-tâche. Le contrat QEMU lance `spin`, une boucle utilisateur sans syscall, puis exige une nouvelle commande du shell : la réactivité obtenue démontre la préemption réelle.

Le premier incrément MOHHOS ajoute une boîte aux lettres **IPC Foundation** par tâche utilisateur. `SYS_IPC_SEND` transmet un payload borné de 96 octets vers un PID utilisateur valide et le noyau inscrit lui-même le PID d’émetteur ; `SYS_IPC_RECV` retire le plus ancien message de la FIFO. Chaque endpoint tient quatre messages, rejette explicitement la saturation et ne bloque jamais le destinataire. Après un envoi réussi, le noyau réalise un handoff coopératif lorsqu’une autre tâche utilisateur est prête : cela permet au destinataire de traiter le message avant que le shell ne retourne dans `SYS_GETS`, cadre Ring 0 non préemptable par IRQ0. Le contrat QEMU lance `ipcserver`, envoie `bonjour`, vérifie l’émetteur puis confirme une boîte aux lettres vide. Ce mécanisme prépare l’externalisation ultérieure des services ; il ne constitue ni un microkernel ni un IPC à capabilities.

Le deuxième incrément construit un premier **médiateur VFS Ring 3** sur cet endpoint. `vfsserver` reçoit une requête `OS_IPC_VFS_READ`, valide un chemin NUL-terminé de 47 octets utiles au plus et refuse `..`, lit au plus 80 octets via l’ABI existante, puis répond au PID expéditeur avec statut et données. Le backend initrd/overlay/ATA demeure néanmoins noyau, et un client peut encore appeler `SYS_READFILE` directement : il s’agit d’un médiateur de politique, non d’un VFS isolé.

Le troisième incrément retire le PID codé en dur de ce parcours. `SYS_SERVICE_REGISTER` associe un nom de 15 octets utiles au plus au PID de l’appelant Ring 3 et `SYS_SERVICE_LOOKUP` retourne le PID d’un propriétaire utilisateur vivant. `vfsserver` publie `vfs`, puis `vfs-read <chemin>` le résout avant l’envoi IPC. Le registre contient huit entrées, refuse les noms invalides et les conflits actifs. Le client VFS cède au plus trois fois le CPU puis relit sa boîte aux lettres, ce qui préserve un protocole non bloquant tout en absorbant le handoff d’ordonnancement.

Le quatrième incrément fixe le cycle de vie : `SYS_SERVICE_UNREGISTER` ne retire qu’un nom détenu par son appelant Ring 3 ; `SYS_EXIT` et `SYS_KILL` retirent toutes les entrées du PID avant la terminaison ou le retrait de la tâche. La purge lazy à la recherche demeure une défense secondaire. Le registre ne confère toujours aucun droit : tout processus utilisateur peut tenter de réserver un nom libre, les capabilities, badges et registre persistant restent absents.

Le cinquième incrément ajoute `request_id` à `os_ipc_payload_t` et `os_ipc_message_t`, sans modifier la charge utile de 96 octets ni les numéros de syscall. La FIFO noyau le copie de bout en bout. `vfs-read` génère un identifiant non nul monotone par session, et `vfsserver` le recopie dans la réponse ; le parseur VFS refuse type, taille ou identifiant discordant. Le client retire et ignore une réponse non corrélée pendant sa fenêtre de trois cessions CPU, ce qui prépare la gestion de plusieurs requêtes mais ne conserve pas encore les messages ignorés ni n’offre un RPC bloquant.

Le sixième incrément introduit `SYS_SERVICE_GRANT` : le propriétaire courant d’un nom peut transférer ce nom à une tâche Ring 3 vivante. Le registre remplace son PID en place, et le nettoyage à `exit` ou `kill` s’applique ensuite au nouveau propriétaire. Un handoff coopératif laisse le bénéficiaire constater immédiatement le transfert avant que le shell ne retourne dans son attente Ring 0. Cela reste un transfert unique, volatile et sans secret : l’ancien propriétaire ne peut plus le révoquer, la découverte demeure publique et aucun token de capability, contrôle d’identité ou audit n’est fourni.

Le septième incrément conserve les messages non corrélés retirés de l’endpoint du shell : `vfs-read` les place dans une file Ring 3 de quatre entrées, puis `ipc-recv` les restitue FIFO. Une réponse VFS dont le type et le `request_id` correspondent est extraite directement de cette file avant toute attente. La capacité est strictement bornée : une saturation renvoie une erreur au lieu d’écraser un message, et aucune file partagée, persistance ou attente bloquante n’est ajoutée.

Le huitième incrément ajoute `vfs-info`, une source synthétique servie directement par `vfsserver` Ring 3 sans `SYS_READFILE`. Il déplace ainsi une première politique de chemin et la construction de sa réponse hors du noyau. Les autres chemins sûrs restent relayés vers le backend initrd/overlay noyau ; il ne s’agit ni d’un montage général ni d’une externalisation des pilotes ou du stockage ATA.

Le neuvième incrément réserve `SYS_VFS_BACKEND_READ` au propriétaire utilisateur vivant du nom `vfs`. `vfsserver` l’utilise pour ses chemins ordinaires et le shell reçoit `OS_VFS_BACKEND_DENIED` par la commande de diagnostic. Cette voie isole le médiateur du syscall générique pour le protocole VFS, sans retirer `SYS_READFILE` des commandes historiques, sans capabilities ni externalisation du backend initrd/overlay.

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

Le shell Ring 3 propose `ls`, `cat`, `mkdir`, `rmdir`, `rm`, `cp`, `mv`, `write`, `append`, `touch`, `stat`, `test`, `grep`, `wc`, `sort`, `head`, `tail`, `ps`, `jobs`, `top`, `spawn`, `yield`, `ipc-send`, `ipc-recv`, `service-publish`, `service-grant`, `service-find`, `vfs-read`, `kill`, `exec`, `mem`, `uptime`, `history`, `alias`, `env`, `ai`, `ai-provider`, `ai-model`, `ai-runtime` et `net-status`. Les opérations de fichiers modifiables passent par l’overlay noyau ; l’initrd demeure en lecture seule. Les programmes empaquetés incluent `shell`, `idle`, `spin`, `ipcserver`, `vfsserver`, `serviceclaim`, `ok`, `fake_ai`, `ai_assistant` et `user_program`.

L’ABI contient les syscalls 0–29 (`MAX_SYSCALLS = 30`), dont `SYS_APPEND`, `SYS_GPT2_GENERATE`, `SYS_IPC_SEND`, `SYS_IPC_RECV`, `SYS_SERVICE_REGISTER`, `SYS_SERVICE_LOOKUP`, `SYS_SERVICE_UNREGISTER`, `SYS_SERVICE_GRANT` et `SYS_VFS_BACKEND_READ`. Les structures IPC ajoutent un `request_id` 32 bits opaque, copié par le noyau sans changer le plafond de charge utile. Le profil local ne nécessite aucun secret ni aucune dépendance réseau à l’exécution.

## Vérification reproductible

```bash
sudo apt-get install -y build-essential gcc-multilib nasm qemu-system-i386 grub-pc-bin xorriso
make clean && make all       # noyau, initrd et disque overlay
make test-all                # 186/186 tests C Unity et robustesse
make integration-qemu        # contrats AOS-022/AOS-024/AOS-025, IPC, VFS différé et transfert Foundation
make iso                     # ISO BIOS/GRUB bootable
make run                     # console curses
make run-gui                 # fenêtre GTK
```

La suite C exécute 186 tests : PMM (17), syscall (48), tâches (21), overlay (8), tokenizer (15), GGUF (5), quantification (5), échantillonnage GPT-2 (4), IPC (6), file IPC différée (4), protocole VFS (6), registre de services (9), shell (25), RAMFS (10) et robustesse GGUF (3). `make integration-qemu` démarre six machines QEMU séparées : le contrat cœur AOS-022 utilise une image overlay de test isolée, le contrat IRQ0 AOS-024 préempte `spin`, le smoke AOS-025 vérifie que le profil OpenAI reste bloqué, le contrat Foundation livre un message à `ipcserver`, le contrat VFS résout `vfs`, conserve un message concurrent `deferred`, exige `request 1 data`, lit `hello.txt` à travers `vfsserver`, restitue le message différé puis vérifie son nettoyage, puis le contrat de transfert publie `demo`, le donne à un autre PID et exige sa suppression après `kill`.

Le build a également produit une ISO GRUB BIOS avec l’initrd GPT-2 local. Avec les actifs disponibles dans l’environnement de vérification, l’ISO est d’environ 481 Mio ; les modèles restent exclus du versionnement.

## Absences importantes

AI-OS ne fournit pas de système de fichiers disque général, de pilote réseau, de pile TCP/IP/TLS, de client OpenAI/Ollama effectif, d’UEFI, de gestion multiprocesseur, de GUI native, de microkernel, d’IPC bloquant, de table de requêtes en attente, de routage général des réponses discordantes, de capabilities, de révocation de transfert, d’identité vérifiée ni des fonctionnalités avancées de la vision MOHHOS. La boîte aux lettres IPC corrélée, `vfsserver` avec source virtuelle et le registre nommé avec transfert sont des mécanismes locaux préparatoires : le backend fichiers reste noyau, l’ABI directe reste accessible aux clients et le registre ne constitue pas un contrôle d’accès. Les rapports historiques conservés dans `docs/` sont des éléments de chronologie et non la description de l’état courant.

## Références

[1] [QEMU, *Network emulation*](https://www.qemu.org/docs/master/system/devices/net.html)
