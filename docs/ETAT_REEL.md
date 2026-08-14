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
| IPC/VFS Foundation MOHHOS | Boîte aux lettres FIFO non bloquante entre tâches Ring 3, 4 entrées par tâche, charge de 96 octets et `request_id` corrélé ; médiateur VFS, sources virtuelles, compteurs volatils et politique de montages `initrd/` lecture seule et `overlay/` écriture médiée |
| Découverte Foundation MOHHOS | Registre volatile de 8 services nommés ; retrait, transfert par propriétaire, révocation VFS, abonnements de service best-effort et nettoyage à la terminaison |
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

Le dixième incrément permet à `vfsserver` de transférer son propre nom via une requête IPC `OS_IPC_VFS_GRANT` et la commande shell `vfs-grant <pid>`. Le contrôle backend suit immédiatement le nouveau propriétaire : `vfsclaim` prouve la lecture autorisée, tandis que l’arrêt de l’ancien serveur ne supprime plus le nom. L’arrêt du nouveau propriétaire le purge. Cette révocation repose sur un PID de registre volatile, pas sur un token de capability, une identité ou une autorité de révocation indépendante.

Le onzième incrément ajoute un registre de montages borné au médiateur. `vfsserver` déclare actuellement le préfixe `initrd/`, l’expose en lecture virtuelle via `vfs-read vfs-mounts` et délègue seulement le suffixe relatif d’un chemin correspondant au backend réservé. Ainsi `vfs-read initrd/hello.txt` atteint `hello.txt`, tandis que `vfs-read hello.txt` retourne `OS_VFS_STATUS_NOT_MOUNTED` et un diagnostic de chemin hors montage. Les sources `vfs-info` et `vfs-mounts` sont toujours produites en Ring 3. La liste est statique, ne monte ni autre backend ni répertoire, et le backend initrd/overlay/ATA reste noyau.

Le douzième incrément ajoute `SYS_SERVICE_NOTIFY` et huit abonnements `(nom, PID)` au plus. Lors d’une publication, d’un transfert, d’un retrait ou d’une purge, le noyau émet un `OS_IPC_SERVICE_EVENT` vers chaque abonné actif avec `sender_pid = 0`, l’ancien PID, le nouveau PID et une raison bornée. Le shell offre `service-watch <nom>` et rend l’événement visible par `ipc-recv`; `serviceclaim` s’abonne à `demo` au lieu de retenter continuellement l’inscription. Cette livraison est best-effort : la boîte IPC de quatre entrées peut être saturée, sans bloquer le registre ni garantir la reprise de l’événement.

Le treizième incrément ajoute `SYS_VFS_BACKEND_WRITE`, réservé au propriétaire vivant du nom `vfs`, et le protocole `OS_IPC_VFS_WRITE`. `vfsserver` annonce désormais `initrd/ ro` et `overlay/ rw`. La commande `vfs-write overlay/nom.txt texte` valide le préfixe, transmet seulement `nom.txt` au backend overlay et exige une réponse corrélée. Une écriture vers `initrd/` ou hors montage est refusée par le médiateur ; le shell ne peut pas appeler directement la voie backend réservée. Chaque requête transporte au plus 44 octets, l’overlay/ATA reste noyau, et le syscall historique d’écriture demeure accessible hors du protocole VFS.

Le quatorzième incrément sépare les lectures backend par source. `SYS_VFS_INITRD_READ` sert exclusivement l’archive initrd pour le montage `initrd/`, tandis que `SYS_VFS_OVERLAY_READ` sert exclusivement l’overlay ATA pour `overlay/`. Les deux voies sont réservées au propriétaire de `vfs`. Le médiateur ne délègue plus les lectures montées au backend générique à repli implicite ; `vfs-read overlay/nom.txt` lit ainsi un fichier créé par `vfs-write` depuis l’overlay, alors que `vfs-read initrd/nom.txt` ne consulte jamais l’overlay.

Le quinzième incrément ajoute `SYS_VFS_OVERLAY_UNLINK`, réservé au propriétaire de `vfs`, ainsi que `OS_IPC_VFS_REMOVE`. `vfs-remove overlay/nom.txt` retire seulement le suffixe relatif `nom.txt` de l’overlay. Une suppression sous `initrd/` ou hors montage est refusée avant l’appel backend ; la commande `vfs-backend-remove-probe` démontre que le shell n’accède pas directement à cette primitive. La réponse est corrélée au `request_id`, mais l’opération reste non transactionnelle et les erreurs d’overlay sont renvoyées au client.

Le seizième incrément ajoute `SYS_VFS_OVERLAY_RENAME` et `OS_IPC_VFS_RENAME`. Les deux chemins de 48 octets remplissent exactement la charge IPC de 96 octets et doivent appartenir au même montage `overlay/`. `vfs-rename overlay/ancien.txt overlay/nouveau.txt` ne transmet donc que les deux suffixes relatifs au backend réservé. Les changements inter-montages ou les chemins non sûrs sont refusés ; `vfs-backend-rename-probe` prouve que le shell ne peut pas appeler la primitive directe. La réponse transporte le statut et le `request_id` originel.

Le dix-septième incrément ajoute des compteurs locaux d’observabilité dans `vfsserver`, exposés sans nouveau syscall par la source virtuelle `vfs-stats` et la commande `vfs-stats`. Les compteurs 32 bits `reads`, `writes`, `removes` et `renames` sont remis à zéro au démarrage du serveur. Toute requête VFS reconnue est comptée avant validation, de sorte que les refus de politique et l’interrogation `vfs-stats` elle-même restent visibles. Ces compteurs ne sont ni persistants, ni atomiques, ni des métriques de sécurité ou de performance.

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

Le shell Ring 3 propose `ls`, `cat`, `mkdir`, `rmdir`, `rm`, `cp`, `mv`, `write`, `append`, `touch`, `stat`, `test`, `grep`, `wc`, `sort`, `head`, `tail`, `ps`, `jobs`, `top`, `spawn`, `yield`, `ipc-send`, `ipc-recv`, `service-publish`, `service-grant`, `service-find`, `service-watch`, `vfs-backend-probe`, `vfs-backend-write-probe`, `vfs-backend-remove-probe`, `vfs-backend-rename-probe`, `vfs-grant`, `vfs-read`, `vfs-stats`, `vfs-write`, `vfs-remove`, `vfs-rename`, `kill`, `exec`, `mem`, `uptime`, `history`, `alias`, `env`, `ai`, `ai-provider`, `ai-model`, `ai-runtime` et `net-status`.
 Les opérations de fichiers modifiables passent par l’overlay noyau ; l’initrd demeure en lecture seule. Les programmes empaquetés incluent `shell`, `idle`, `spin`, `ipcserver`, `vfsserver`, `serviceclaim`, `vfsclaim`, `ok`, `fake_ai`, `ai_assistant` et `user_program`.

L’ABI contient les syscalls 0–35 (`MAX_SYSCALLS = 36`), dont `SYS_APPEND`, `SYS_GPT2_GENERATE`, `SYS_IPC_SEND`, `SYS_IPC_RECV`, `SYS_SERVICE_REGISTER`, `SYS_SERVICE_LOOKUP`, `SYS_SERVICE_UNREGISTER`, `SYS_SERVICE_GRANT`, `SYS_VFS_BACKEND_READ`, `SYS_SERVICE_NOTIFY`, `SYS_VFS_BACKEND_WRITE`, `SYS_VFS_INITRD_READ`, `SYS_VFS_OVERLAY_READ`, `SYS_VFS_OVERLAY_UNLINK` et `SYS_VFS_OVERLAY_RENAME`.
 Les structures IPC ajoutent un `request_id` 32 bits opaque, copié par le noyau sans changer le plafond de charge utile. Les événements de service utilisent ce même transport, avec un émetteur noyau (`sender_pid = 0`) et `request_id = 0`. Le profil local ne nécessite aucun secret ni aucune dépendance réseau à l’exécution.

## Vérification reproductible

```bash
sudo apt-get install -y build-essential gcc-multilib nasm qemu-system-i386 grub-pc-bin xorriso
make clean && make all       # noyau, initrd et disque overlay
make test-all                # 198/198 tests C Unity et robustesse
make integration-qemu        # contrats AOS-022/AOS-024/AOS-025, IPC, VFS lecture-écriture-suppression-renommage médié, révocation, transfert et notifications Foundation
make iso                     # ISO BIOS/GRUB bootable
make run                     # console curses
make run-gui                 # fenêtre GTK
```

La suite C exécute 198 tests : PMM (17), syscall (48), tâches (21), overlay (8), tokenizer (15), GGUF (5), quantification (5), échantillonnage GPT-2 (4), IPC (6), file IPC différée (4), protocole VFS (14), registre de services (13), shell (25), RAMFS (10) et robustesse GGUF (3). `make integration-qemu` démarre six machines QEMU séparées : le contrat cœur AOS-022 utilise une image overlay de test isolée, le contrat IRQ0 AOS-024 préempte `spin`, le smoke AOS-025 vérifie que le profil OpenAI reste bloqué, le contrat Foundation livre un message à `ipcserver`, le contrat VFS résout `vfs`, expose `initrd/ ro` et `overlay/ rw`, refuse les écritures, suppressions et renommages hors montage, réserve les backends au serveur, conserve un message concurrent `deferred`, vérifie la lecture/écriture/suppression/renommage corrélée et source-spécifique, les sources virtuelles et les compteurs `vfs-stats`, puis le transfert, la révocation de l’ancien serveur et la purge du nouveau propriétaire. Le contrat de transfert publie `demo`, abonne le shell, cède explicitement le CPU après le grant, vérifie la réaction événementielle de `serviceclaim`, puis l’événement de purge après `kill`. Les cadences du smoke cœur, extras et du contrat VFS sont respectivement 0,65 s, 0,80 s et 0,55 s pour limiter les doubles frappes PS/2 sous QEMU TCG.

Le build a également produit une ISO GRUB BIOS avec l’initrd GPT-2 local. Avec les actifs disponibles dans l’environnement de vérification, l’ISO est d’environ 481 Mio ; les modèles restent exclus du versionnement.

## Absences importantes

AI-OS ne fournit pas de système de fichiers disque général, de pilote réseau, de pile TCP/IP/TLS, de client OpenAI/Ollama effectif, d’UEFI, de gestion multiprocesseur, de GUI native, de microkernel, d’IPC bloquant, de table de requêtes en attente, de routage général des réponses discordantes, de capabilities, d’identité vérifiée ni des fonctionnalités avancées de la vision MOHHOS. La boîte aux lettres IPC corrélée, `vfsserver` avec sources virtuelles et compteurs locaux, montages statiques, écriture, suppression et renommage overlay médiés, lecture source-spécifique, ainsi que le registre nommé avec transfert, révocation VFS et notifications best-effort, sont des mécanismes locaux préparatoires : le backend fichiers reste noyau, l’ABI directe reste accessible aux clients et le registre ne constitue pas un contrôle d’accès. L’écriture VFS est limitée à 44 octets par requête et les mutations VFS n’apportent ni transaction, ni verrouillage, ni autorisation par répertoire. Les statistiques ne sont ni persistantes, ni atomiques, ni horodatées et peuvent déborder. Les événements ne sont ni persistants, ni accusés, ni garantis lorsque la boîte IPC est pleine. Les rapports historiques conservés dans `docs/` sont des éléments de chronologie et non la description de l’état courant.

## Références

[1] [QEMU, *Network emulation*](https://www.qemu.org/docs/master/system/devices/net.html)
