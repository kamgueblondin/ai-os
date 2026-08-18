# TODO - Correction AI-OS Shell Utilisateur

> **État réel (août 2026).** Le shell utilisateur Ring 3 **se lance** et le clavier **répond** (correctif EOI IRQ0). Les cases des phases 5-6 ci-dessous sont mises à jour. Le diagnostic d'origine (phases 1-4) est **conservé** : il décrit correctement un état antérieur. Référence : [ETAT_REEL.md](ETAT_REEL.md).

## Phase 1: Récupération et configuration du projet ✅
- [x] Cloner le projet depuis GitHub
- [x] Examiner la structure du projet
- [x] Analyser le Makefile
- [x] Identifier les fichiers sources principaux

## Phase 2: Analyse du code existant ✅
- [x] Analyser le code du kernel principal
- [x] Examiner le code du shell userspace
- [x] Comprendre la gestion des tâches et processus
- [x] Analyser les appels système
- [x] Examiner les logs de debug existants

## Phase 3: Identification des problèmes ✅
- [x] Identifier pourquoi l'interface reste figée en mode utilisateur
- [x] Analyser les problèmes de gestion des processus
- [x] Vérifier les appels système et la communication kernel/userspace
- [x] Identifier les problèmes de redémarrage/crash

### Problèmes identifiés:
1. **Transition Kernel->Userspace manquante**: Le système reste en mode simulation au lieu de passer au shell utilisateur
2. **Gestion des interruptions clavier**: sys_gets() peut bloquer indéfiniment avec hlt
3. **Context switch incomplet**: Pas de switch_to_userspace() implémenté
4. **Configuration timer**: Conflits d'interruptions lors de la transition

## Phase 4: Tests et débogage avec make run ✅
- [x] Compiler et tester le système actuel
- [x] Analyser les logs en temps réel
- [x] Identifier les points de blocage
- [x] Tester différentes configurations

### Résultats des tests:
- ✅ **Compilation réussie** après installation de nasm, gcc-multilib et qemu
- ✅ **Système stable** - Plus de redémarrage en boucle
- ✅ **Timer fonctionnel** - Ticks réguliers à 100Hz
- ❌ **Mode simulation seulement** - Le système reste en mode kernel au lieu de passer au shell utilisateur *(constat d'alors)*
- 📝 **Point de blocage identifié**: Le code reste dans la boucle de simulation au lieu d'exécuter le shell utilisateur

*Mise à jour août 2026 :* le shell ELF userspace est lancé ; le "mode simulation seulement" ne s'applique plus. Voir phase 5.

## Phase 5: Correction et refactorisation ✅ (août 2026)
- [x] Corriger les problèmes identifiés (transition userspace via `jump_to_task`, EOI PIC avant `schedule()`)
- [x] Refactoriser le code si nécessaire (planification uniquement sur `g_reschedule_needed`)
- [x] Améliorer la stabilité du système (plus de reboot systématique après le timer)
- [x] Optimiser la communication kernel/userspace (syscalls GETS/EXEC fonctionnels)

### Reste ouvert (hors périmètre "shell qui démarre")
- [x] Commandes listées dans `help` branchées dans `execute_builtin_command` (overlay noyau + initrd ; `procsim.c` n'alimente plus `ps`/`kill`)
- [x] `ls` / `cat` / `ps` / `kill` / `uptime` / `mem` : syscalls noyau (initrd + overlay + `task.c` + PIT + PMM)
- [x] Overlay noyau RAM : `mkdir` / `rm` / `cp` (fichier et dossier via `SYS_COPY`) / `mv` (fichier et dossier via `SYS_RENAME`) / `write` / `append` (`SYS_APPEND`) / `touch` / `echo >` visibles par `ls`/`cat` (initrd toujours read-only)
- [x] Overlay persisté : snapshot AIOV V2 ATA PIO LBA28 sur disque IDE QEMU (`write` survit à un reboot) ; volume FAT16 lecture seule à partir du LBA 64
- [x] `spawn` / `yield` coopératifs (cadre syscall user) et préemption IRQ0 sûre entre tâches Ring 3
- [x] `exec` bloquant : parent `TASK_WAITING`, enfant reveille via `SYS_EXIT` (plus de `int $0x30` noyau)
- [x] AOS-020…026 : sonde GGUF, BPE UTF-8, contrats QEMU, overlay V2, IRQ0, stub OpenAI, FAT16 lecture seule
- [x] Kernels GGUF Q3_K/Q4_K/Q6_K, index et mapping ; génération shell encore FP32
- [ ] Inférence GGUF bout-en-bout et latence locale &lt; 1 s
- [ ] Écriture FAT, LFN et FAT32 — [aos_fat_volume.md](aos_fat_volume.md) ; pas ext2
- [x] Pilote NE2000 ISA et codecs ARP/IPv4/UDP/DHCP/DNS/TCP/TLS record (lots 113–154)
- [ ] Bail DHCP live, socket TCP utilisateur, handshake TLS et client OpenAI effectif
- [x] Contrats QEMU dans `tests/integration` (cœur, IRQ0, fournisseur, NE2000, IPC, VFS, services)

## Phase 6: Tests finaux et soumission sur GitHub ✅ (août 2026)
- [x] Tests complets du système corrigé (`make test-all` : 299 Unity ; `make qemu-smoke` et `make integration-qemu`)
- [x] Validation du fonctionnement en mode utilisateur (QEMU GTK + `sendkey`)
- [x] Commit et push des corrections sur GitHub
- [x] Documentation des corrections apportées ([ETAT_REEL.md](ETAT_REEL.md))

## Problème identifié (historique - phases 1 à 4)
Le mode simulation fonctionnait parfaitement mais le passage au mode utilisateur échoue - l'interface reste figée. Besoin d'analyser les appels système et la gestion des processus.

**Statut 2026 :** ce blocage est levé. Le kernel pose `g_reschedule_needed` puis le timer appelle `schedule()` une fois vers le shell ELF.


## Mise à jour réseau TCP — août 2026

Les lots réseau AOS-132 à AOS-150 sont désormais intégrés progressivement et validés par CI. Le pilote NE2000 sait lire la PROM MAC, émettre en PIO, sonder la réception et traiter IRQ3 ; ARP statique et actif, DHCP et DNS A sont disponibles dans leurs chemins caller-owned. AOS-147 construit un SYN IPv4 TCP, AOS-148 expose les segments TCP reçus, AOS-149 valide strictement un SYN-ACK et AOS-150 construit le premier ACK avec un état `SYN_SENT`/`ESTABLISHED` appartenant à l’appelant.

| Domaine | Statut réel |
|---|---|
| Ethernet, ARP, DHCP, DNS A | Implémentés et couverts par tests Unity / smokes NIC. |
| TCP SYN, SYN-ACK, ACK, payload, séquence, retransmission bornée | Implémentés (AOS-147…154) ; 299 tests verts. |
| Timer RTO, congestion, fermeture TCP, API socket | Non implémentés. |
| Handshake TLS, X.509, HTTP, OpenAI | Non fonctionnels de bout en bout. |

Cette section remplace toute interprétation de la ligne générique « Pilote NIC, DHCP, DNS, TCP/TLS et client OpenAI effectif » : le pilote et les codecs jusqu’à AOS-154 sont livrés, tandis que le bail live, le handshake TLS et le client LLM restent hors périmètre.

Références détaillées : [AOS-149](aos149_tcp_synack_validation.md) et [AOS-150](aos150_tcp_first_ack.md).

### AOS-151 — émission du premier ACK via NE2000

AOS-151 est implémenté localement : `ne2k_tcp_ack` utilise le cache ARP caller-owned, construit Ethernet/IPv4/TCP, calcule les checksums et transmet via `ne2k_tx_submit`. Le test NE2000 valide les ports, séquences, ACK et adresses MAC. La validation complète et la PR restent à effectuer après la non-régression.

Le prochain lot logique est AOS-152 : codec TCP avec données caller-owned et émission bornée d’un segment ACK+payload. Cela ne constitue pas encore un client HTTP, TLS ou LLM fonctionnel.

### AOS-152 — données TCP caller-owned

AOS-152 ajoute le codec `ACK+payload` et `ne2k_tcp_data`, avec longueur IPv4 exacte, checksums TCP/IPv4 et contrôles de capacité. La suite locale atteint 297 tests verts. Le smoke `qemu-ai-provider` a échoué une première fois sur l’absence de sortie `Runtime IA bare-metal`, puis a réussi lors d’une relance immédiate ; le smoke `qemu-ne2k-status` est vert. Cette anomalie de smoke IA reste à surveiller séparément du chemin TCP.

### AOS-153/AOS-154 — séquences, ACK reçus et retransmission bornée

AOS-153 fait progresser explicitement `local_sequence` après envoi confirmé et `remote_sequence` après acceptation d’un payload en séquence. AOS-154 conserve une vue caller-owned du dernier payload et borne le nombre de retransmissions autorisées. Aucun timer, RTO, mécanisme de congestion ou stockage copié n’est introduit. La validation complète atteint désormais 299 tests verts, avec build i386 et deux smokes QEMU réussis.

### AOS-155 à AOS-158 — retransmission, ACK final et fermeture TCP

Les lots ajoutent la retransmission NE2000 caller-owned, la confirmation et purge du payload pending, le codec et l’émission FIN+ACK, puis les transitions FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT et CLOSED. La validation complète atteint 301 tests verts, avec build i386, smoke IA et smoke NE2000 réussis. Les timers RTO, la congestion, TLS, HTTP et l’appel LLM en ligne restent hors périmètre fonctionnel.

### AOS-159 à AOS-162 — envoi suivi d’ACK, réception, fenêtre et checksums

AOS-159 ajoute la construction data suivie d’un pending caller-owned avant commit TX. AOS-160 ajoute la réception TCP NE2000 bornée avec copie vers le buffer appelant. AOS-161 ajoute la fenêtre de réception explicite. AOS-162 valide les checksums IPv4 et TCP en réception. Validation : 304 tests verts, build i386 réussi, smoke IA réussi et smoke NE2000 réussi.

### AOS-163 à AOS-165 — polling TCP et orchestration RX

AOS-163 ajoute `ne2k_tcp_poll`, AOS-164 formalise le retour non bloquant RX vide avec longueur nulle, et AOS-165 regroupe les contrôles de bornes, checksums, séquence et fenêtre dans le chemin de réception caller-owned. Le prochain incrément reste la génération d’un ACK automatique après payload accepté.

Validation AOS-163/AOS-165 : **305/305 tests verts**, build i386 réussi, `qemu-ai-provider` réussi et `qemu-ne2k-status` réussi.

### AOS-166/AOS-167 — polling TCP suivi d’ACK

`ne2k_tcp_poll_ack` reçoit et valide un payload TCP, le publie dans le buffer appelant puis émet son ACK via le cache ARP caller-owned. RX vide retourne `1` sans émission ; une erreur ARP/TX est propagée après acceptation afin que l’appelant puisse décider d’une nouvelle tentative. Documentation : `docs/aos166_167_tcp_poll_ack.md`.

Validation AOS-166/AOS-167 : **305/305 tests verts**, build i386 réussi, smoke `qemu-ai-provider` réussi et smoke `qemu-ne2k-status` réussi.

### AOS-168/AOS-169 — FIN→ACK et contrôle TCP

Le polling FIN caller-owned valide la trame, effectue la transition de fermeture et émet l’ACK via NE2000. Les ACK purs ne déclenchent pas de réponse automatique afin d’éviter les boucles. Le framing TLS 1.2 existe séparément, mais son handshake cryptographique et son raccordement TCP restent à implémenter.

Validation AOS-168/AOS-169 : **305/305 tests verts**, build i386 réussi, smoke `qemu-ai-provider` réussi et smoke `qemu-ne2k-status` réussi.

### AOS-170 — ClientHello TLS 1.2 minimal

Le codec TLS record construit maintenant un ClientHello minimal caller-owned avec random fourni, sans génération cryptographique ni négociation complète. SNI/ALPN, dérivation de clés, chiffrement, X.509, HTTP et appels LLM en ligne restent hors périmètre fonctionnel.

Validation AOS-170 : **306/306 tests verts**, build i386 réussi, smoke `qemu-ai-provider` réussi et smoke `qemu-ne2k-status` réussi.

### AOS-171 — composition TLS sur TCP

`net_tcp_connection_build_tls_record` construit un record TLS dans un buffer caller-owned puis l’encapsule dans un segment TCP pending, réutilisable pour commit et retransmission. Le raccordement ne fournit encore ni cryptographie TLS, ni certificat, ni HTTP ou appel LLM en ligne.

Validation AOS-171 : **307/307 tests verts**, après ajout de `net_tls_record.c` aux chemins de linkage TCP/NE2000 du harness ; build i386 réussi, smoke `qemu-ai-provider` réussi et smoke `qemu-ne2k-status` réussi.

### AOS-172/AOS-173 — parsing TLS depuis TCP

Le parsing stream TLS distingue les fragments incomplets et les records complets. La connexion TCP accepte un record complet uniquement lorsqu’il occupe exactement le payload reçu, puis avance le sequence distant ; les buffers d’assemblage restent caller-owned.

Validation AOS-172/AOS-173 : **309/309 tests verts**, build i386 réussi, smoke `qemu-ai-provider` réussi et smoke `qemu-ne2k-status` réussi.

### AOS-174 — accumulateur de fragments TLS

L’état d’assemblage TLS est désormais caller-owned : les fragments TCP sont bornés par la capacité fournie, le record incomplet reste non publié et le record complet est parsé sans allocation. Validation : **310/310 tests verts**, build i386 réussi, smoke IA réussi et smoke NE2000 réussi.

### AOS-175 — parsing ServerHello TLS

Le ServerHello TLS minimal est désormais parsé sans copie dans une vue caller-owned, avec contrôles de longueur, version, random, session ID, cipher suite et compression. Validation : **311/311 tests verts**, build i386 réussi, smoke IA réussi et smoke NE2000 réussi. Extensions TLS, dérivation de clés, chiffrement, X.509, HTTP et appels LLM en ligne restent hors périmètre.

### AOS-176 — état minimal du handshake TLS

Le handshake TLS caller-owned suit désormais l’ordre `IDLE → CLIENT_HELLO_SENT → SERVER_HELLO_RECEIVED`, conserve la suite et une vue du random serveur, et rejette les transitions invalides. Validation : **312/312 tests verts**, build i386 réussi, smoke IA réussi et smoke NE2000 réussi. La cryptographie, Finished, X.509, HTTP et les appels LLM en ligne restent hors périmètre.


### AOS-177 — Extensions ServerHello TLS

Le parseur `ServerHello` expose désormais le bloc d’extensions TLS via une vue caller-owned (`extensions` et `extensions_length`). Il accepte l’absence d’extensions ou un vecteur correctement borné, et rejette toute longueur incohérente sans `kmalloc`. Le faux échec de `test_net_tls_record` provenait d’un dépassement de pile dans le test, corrigé par un buffer dimensionné pour le cas d’extension.

Validation AOS-177 : **312/312 tests verts**, build i386 réussi, `qemu-ai-provider` réussi et `qemu-ne2k-status` réussi. Le décodage sémantique des extensions, X.509, la dérivation de clés, le chiffrement TLS, Finished, HTTP et les appels LLM sécurisés de bout en bout restent non implémentés. Référence : [aos177_tls_server_hello_extensions.md](aos177_tls_server_hello_extensions.md).


### AOS-178 — Parsing Certificate TLS 1.2

Le message `Certificate` TLS 1.2 est maintenant validé sans allocation dynamique. Le codec contrôle les longueurs 24 bits du corps, de la liste et de chaque certificat, publie la première chaîne via une vue caller-owned et parcourt les entrées supplémentaires. L’automate accepte Certificate uniquement après `SERVER_HELLO_RECEIVED` et passe à `CERTIFICATE_RECEIVED`.

Validation ciblée : **8/8 tests TLS verts**. La suite complète, le build i386 et les smokes QEMU restent à confirmer avant publication. La validation X.509, la dérivation de clés, le chiffrement TLS, Finished, HTTP et les appels LLM sécurisés de bout en bout restent non implémentés. Référence : [aos178_tls_certificate_parse.md](aos178_tls_certificate_parse.md).

### AOS-179 — ServerHelloDone TLS 1.2

Le message `ServerHelloDone` est désormais validé comme handshake de type 14 à corps vide. L’automate caller-owned l’accepte uniquement après `CERTIFICATE_RECEIVED`, passe à `SERVER_HELLO_DONE_RECEIVED` et rejette les transitions hors ordre ou répétées. Aucun `kmalloc`, buffer interne ou copie n’est introduit.

Validation AOS-179 : **316/316 tests verts**, build i386 réussi, `qemu-ai-provider` réussi et `qemu-ne2k-status` réussi. Le harnais IA réessaie une seule fois une commande en cas de perte ponctuelle d’un caractère par l’injection clavier QEMU. Les échanges de clés, la dérivation de secrets, X.509, le chiffrement TLS, Finished, HTTP et les appels LLM sécurisés de bout en bout restent non implémentés. Référence : [aos179_tls_server_hello_done.md](aos179_tls_server_hello_done.md).


### AOS-180 à AOS-184 — macro-lot messages serveur TLS, transcript et TCP

Le handshake TLS dispose maintenant d’une vue générique, d’un accumulateur de fragments et d’un transcript borné fournis par l’appelant. Le macro-lot parse `ServerKeyExchange` ECDHE et `CertificateRequest`, étend l’automate jusqu’à `ServerHelloDone`, puis connecte le dispatch serveur au record TLS reçu sur TCP. Le chemin TCP est transactionnel : une erreur de type de record ou de handshake restaure sa séquence et sa fenêtre de réception.

Validation AOS-180/AOS-184 : **320/320 tests verts**. Les 13 tests TLS ciblés et le test TCP d’intégration couvrent le framing, les limites, les transitions, le transcript et la restauration transport. Aucun `kmalloc` n’est introduit. La validation X.509, la signature ServerKeyExchange, ECDHE, la dérivation de clés, le chiffrement TLS, Finished, HTTP et les appels LLM sécurisés de bout en bout restent non implémentés. Référence : [aos180_184_tls_server_macro.md](aos180_184_tls_server_macro.md).


### AOS-185 à AOS-188 — flight client TLS après ServerHelloDone

Le macro-lot construit maintenant `Certificate` client vide, `ClientKeyExchange`, `ChangeCipherSpec` et `Finished` dans des buffers caller-owned. L’automate distingue le chemin avec `CertificateRequest` du chemin sans certificat client, puis impose l’ordre ClientKeyExchange → ChangeCipherSpec → Finished. Le `verify_data` est fourni par l’appelant : aucune dérivation ECDHE, PRF TLS, signature ou cryptographie de record n’est simulée.

Validation AOS-185/AOS-188 : **321/321 tests verts**. La dérivation du secret partagé, l’authentification X.509, la vérification de signature, le PRF TLS 1.2, le chiffrement AEAD, Finished authentique, HTTP et les appels LLM sécurisés de bout en bout restent non implémentés. Référence : [aos185_188_tls_client_flight.md](aos185_188_tls_client_flight.md).


### AOS-189 à AOS-192 — PRF TLS 1.2, master secret et Finished

Le PRF TLS 1.2 HMAC-SHA256 est maintenant disponible avec workspace caller-owned, ainsi que le hash SHA-256 du transcript, la dérivation d’un master secret de 48 octets à partir d’un premaster secret fourni et le `verify_data` client Finished de 12 octets. Les vecteurs déterministes valident les quatre primitives. Les dépendances SHA-256 sont déclarées pour les tests TLS, TCP et NE2000 dans les deux couches du harness.

Validation AOS-189/AOS-192 : **322/322 tests verts**. ECDHE réel, le premaster secret, X.509, la signature ServerKeyExchange, les clés de trafic, AEAD, Finished serveur, HTTP et les appels LLM sécurisés de bout en bout restent non implémentés. Référence : [aos189_192_tls_prf_finished.md](aos189_192_tls_prf_finished.md).


### AOS-193 à AOS-196 — post-flight serveur ChangeCipherSpec et Finished

Le post-flight serveur est maintenant encadré par le parsing strict de ChangeCipherSpec (`0x01`) puis de Finished (12 octets). La comparaison de `verify_data` est effectuée sans sortie anticipée. L’intégration TCP accepte les deux records de manière transactionnelle, ajoute Finished validé au transcript et restaure la connexion, l’automate et la longueur du transcript en cas d’erreur.

Validation AOS-193/AOS-196 : **324/324 tests verts**. Le code ne déchiffre ni n’authentifie de records TLS chiffrés : ECDHE, X.509, clés de trafic, AEAD, HTTP et appels LLM sécurisés de bout en bout restent non implémentés. Référence : [aos193_196_tls_server_postflight.md](aos193_196_tls_server_postflight.md).


### AOS-197 à AOS-200 — expansion des clés AES-128-GCM

Une fonction caller-owned dérive maintenant le bloc de 40 octets `key expansion` pour AES-128-GCM à partir du master secret et de `server_random || client_random`. Elle publie les vues client/server write keys de 16 octets et les IV fixes de 4 octets. Le vecteur HMAC-SHA256 couvre le bloc entier et les bornes de capacité.

Validation AOS-197/AOS-200 : **325/325 tests verts**. AES-128-GCM, nonce explicite, tags, chiffrement/déchiffrement de records, ECDHE réel, X.509, HTTP et les appels LLM TLS sécurisés de bout en bout restent non implémentés. Référence : [aos197_200_tls_key_block.md](aos197_200_tls_key_block.md).


### AOS-201 à AOS-208 — AES-128-GCM, records protégés et TCP transactionnel

AES-128, GHASH et GCM sont maintenant implémentés en freestanding. Les records TLS 1.2 utilisent le nonce explicite de huit octets, l’AAD de séquence TLS et un tag de seize octets. Les sessions caller-owned sélectionnent les clés client/serveur, avancent les séquences après succès seulement et les adaptateurs TCP restaurent les états TCP/TLS en cas d’échec de chiffrement, d’authentification ou de déchiffrement.

Validation AOS-201/AOS-208 : **330/330 tests verts**, build i386 réussi, smokes `qemu-ai-provider` et `qemu-ne2k-status` réussis. ECDHE réel, X.509, vérification `ServerKeyExchange`, validation des suites négociées, orchestration TLS de production, HTTP et appels LLM sécurisés de bout en bout restent à implémenter. Référence : [aos201_208_tls_aes_gcm.md](aos201_208_tls_aes_gcm.md).


### AOS-209 à AOS-216 — DER/X.509 minimal caller-owned

Un lecteur DER borné et un parseur X.509 minimal publient maintenant sans copie le TBSCertificate, le serial, issuer, subject, dates de validité et la clé publique RSA du certificat serveur. Le handshake peut déclencher explicitement cette analyse après réception de Certificate. Les longueurs tronquées, indéfinies ou incohérentes sont rejetées.

Validation AOS-209/AOS-216 : **332/332 tests verts**, build i386 réussi. La chaîne de confiance, les dates, l’hôte, les usages, les signatures de certificats, ECDHE réel, la signature ServerKeyExchange et l’orchestration TLS de production restent à implémenter. Référence : [aos209_216_x509_der.md](aos209_216_x509_der.md).


### AOS-217 — Interface RSA PKCS#1 v1.5 SHA-256 : état

L’interface caller-owned `rsa_pkcs1_v15_sha256_verify` est réservée pour la future vérification de signature. Elle ne contient encore ni bigint, ni exponentiation modulaire, ni décodage PKCS#1, ni tests RSA, ni intégration `ServerKeyExchange` ; elle ne fournit donc aucune authentification. Référence : [aos217_rsa_interface_status.md](aos217_rsa_interface_status.md).


### AOS-225 à AOS-232 — bigint multi-limb, RSA PKCS#1 v1.5 et ServerKeyExchange

Le noyau contient désormais une arithmétique bigint caller-owned multi-limb, une réduction modulaire binaire, une exponentiation publique et `rsa_pkcs1_v15_sha256_verify`. La vérification exige un workspace de `7 × ceil(taille_module/4)` limbs fourni et aligné par l’appelant ; elle ne recourt à aucune allocation dynamique. Les tests couvrent une signature RSA-512 PKCS#1 v1.5 SHA-256 valide, les rejets de digest et de signature falsifiés, ainsi qu’une exponentiation à deux limbs.

Une API TLS authentifiée `net_tls_handshake_accept_server_key_exchange_rsa` vérifie désormais `SHA-256(client_random || server_random || ServerECDHParams)` avec la clé RSA extraite du certificat, avant la transition de l’automate. Le dispatcher historique ne reçoit pas encore le random client ni le workspace RSA et reste donc un chemin de parsing non authentifié ; il ne faut pas l’employer comme preuve d’un handshake TLS valide. Référence : [aos225_232_rsa_verify.md](aos225_232_rsa_verify.md).

Les validations de chaîne X.509 et de nom d’hôte, ECDHE réel, les signatures ECDSA, le secret partagé, le raccordement du dispatcher TLS authentifié, HTTP et les appels LLM TLS de bout en bout restent non implémentés.


### AOS-233 à AOS-240 — flux TLS authentifié, X.509 RSA et ECDHE structurel

Le dispatcher `net_tls_handshake_accept_server_message_authenticated` reçoit désormais explicitement le `client_random`, le transcript et le workspace RSA caller-owned. Il parse et transcrit `ServerHello`, analyse `Certificate` en DER/X.509, impose une clé `rsaEncryption` valide, puis vérifie la signature RSA/SHA-256 de `ServerKeyExchange` avant toute transition d’état. L’état du handshake et le transcript sont restaurés transactionnellement sur erreur.

Les paramètres ECDHE font l’objet d’une validation de forme pour secp256r1 (point non compressé de 65 octets) et X25519 (32 octets). Cette validation ne réalise ni multiplication de courbe, ni validation d’appartenance du point, ni dérivation de secret. Le flux historique reste un parseur non authentifié et ne doit pas être confondu avec le nouveau dispatcher. Référence : [aos233_240_tls_authenticated_flow.md](aos233_240_tls_authenticated_flow.md).

La chaîne de confiance, les dates, le nom d’hôte, les usages, les signatures de certificats, ECDSA, ECDHE P-256/X25519 réel, le secret partagé, HTTP et les appels LLM HTTPS de bout en bout restent non implémentés.


### AOS-241 à AOS-248 — X25519, ECDHE et secret partagé TLS

Le noyau contient une implémentation caller-owned de X25519 sur Curve25519 : ladder Montgomery, clamp, clé publique, secret partagé et rejet du résultat nul. Le calcul emploie un workspace fixe de 136 limbs de 32 bits et ne fait appel à aucune allocation dynamique. Le vecteur RFC 7748 est couvert par `test_x25519` [1].

Le ClientHello offre désormais `TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256` (`0xC02F`), X25519 et RSA/SHA-256. Le dispatcher authentifié impose cette suite, exige X25519 au ServerKeyExchange signé, puis fournit la préparation du ClientKeyExchange et la dérivation du master secret depuis le secret partagé. Référence : [aos241_248_x25519_ecdhe.md](aos241_248_x25519_ecdhe.md).

**Limite critique :** le backend bigint modulaire est fonctionnel mais non constant-temps. X25519 ne doit pas encore protéger un secret dans un contexte hostile. Le transcript automatique du ClientKeyExchange, la chaîne X.509, les dates, le nom d’hôte, les signatures de certificats, ECDSA, l’achèvement TLS associé à ce contexte, HTTP et les appels LLM HTTPS de bout en bout restent non implémentés.

[1] [RFC 7748 — Elliptic Curves for Security](https://datatracker.ietf.org/doc/html/rfc7748)


### AOS-249 à AOS-256 — flight client X25519 transactionnel

L’API `net_tls_x25519_client_flight_build` construit désormais le flight client TLS dans des buffers caller-owned : ClientKeyExchange X25519, ChangeCipherSpec et Finished AES-128-GCM. Elle ajoute ClientKeyExchange puis Finished au transcript, dérive le master secret et le key block depuis le contexte X25519, initialise la session AEAD et publie trois records totalisant 93 octets.

La construction est transactionnelle : état du handshake, contexte X25519, transcript et longueur de sortie sont restaurés ou annulés sur erreur. Le chemin refuse actuellement les certificats clients pour ne pas émettre un flight incomplet. Référence : [aos249_256_x25519_client_flight.md](aos249_256_x25519_client_flight.md).

Le transport TCP de ce flight, la validation du ChangeCipherSpec/Finished serveur dans ce même contexte, la chaîne X.509, les dates, le nom d’hôte, les certificats clients, HTTP et les appels LLM HTTPS de bout en bout restent non implémentés. X25519 reste non constante-temps sur le backend bigint actuel.


### AOS-257 à AOS-264 — transport TCP du flight X25519 et post-flight serveur

Le transport TCP expose désormais `net_tcp_connection_build_tls_x25519_flight`, qui encapsule le flight client X25519 transactionnel dans un segment TCP et restaure connexion, handshake, contexte X25519, transcript, session, secrets et longueur publiée sur erreur. Le codec TLS calcule aussi le `server finished`, tandis que `net_tcp_connection_accept_tls_x25519_postflight` accepte le ChangeCipherSpec serveur puis déchiffre, vérifie et transcrit le Finished AES-GCM.

Le test de transport couvre le rollback d’une capacité TCP insuffisante, le suivi des séquences TCP, le rejet transactionnel d’un tag AES-GCM falsifié, le Finished serveur valide et le premier record applicatif chiffré après handshake complet. Référence : [aos257_264_tcp_tls_x25519.md](aos257_264_tcp_tls_x25519.md).

La connexion automatique entre le réassembleur TCP de production et cette orchestration, la chaîne X.509, les dates, le nom d’hôte, les certificats clients, HTTP, l’authentification HTTP, la reprise de session et les appels LLM HTTPS de bout en bout restent non implémentés. Le backend X25519/bigint reste non constante-temps.


### AOS-265 à AOS-272 — flux TCP/TLS authentifié et framing HTTP

Le contexte caller-owned `net_tcp_tls_stream_t` réassemble désormais record et message handshake TLS à travers des fragments TCP, puis applique le dispatcher RSA authentifié. Les fragments incomplets sont conservés et signalés par le statut `1`; un message invalide restaure connexion, handshake, transcript et accumulateurs. Le test couvre un `ServerKeyExchange` RSA authentifié sur deux fragments et le rollback d’une signature falsifiée.

Le module `net_http_tls` construit un GET HTTP/1.1 minimal, le chiffre via la session TLS AES-GCM/TCP et ouvre transactionnellement une réponse HTTP/1.1 complète déjà déchiffrée. Référence : [aos265_272_tls_stream_http.md](aos265_272_tls_stream_http.md).

Le flux ne prend encore en charge qu’un message handshake par record et une réponse HTTP complète dans un seul plaintext. La chaîne X.509, les dates, le nom d’hôte, les certificats clients, `Content-Length`, chunked, les réponses streaming, HTTP POST, l’authentification HTTP, les appels LLM et HTTPS de production complet restent non implémentés. Le backend X25519/bigint reste non constante-temps.


### AOS-273 à AOS-280 — réponse HTTP Content-Length progressive

Le module HTTP/TLS expose désormais `net_http_response_accumulator_t` et `net_http_tls_open_response_stream`. Ils accumulent une réponse HTTP/1.1 sur plusieurs records AES-GCM, exigent un `Content-Length` décimal unique borné à 65 535, et publient une vue body uniquement lorsque la taille reçue correspond exactement à la taille annoncée. Les erreurs HTTP, TCP ou AEAD restaurent le contexte de l’appel.

Le test couvre une réponse `200` dont le body de cinq octets arrive en deux records TLS successifs. Référence : [aos273_280_http_content_length_stream.md](aos273_280_http_content_length_stream.md).

`Transfer-Encoding: chunked`, les réponses terminées par fermeture, trailers, compression, HTTP/2, POST, authentification applicative, pagination, streaming LLM, handshake de production, chaîne X.509, dates, nom d’hôte et backend X25519 constante-temps restent non implémentés.


### AOS-281 à AOS-288 — HTTP POST JSON borné sur TLS

La couche HTTP/TLS fournit `net_http_build_post_json` et `net_http_tls_build_post_json`. La première construit un POST HTTP/1.1 caller-owned avec `Host`, `Content-Type: application/json`, `Content-Length` décimal et `Connection: close`; la seconde chiffre ce plaintext dans un record TLS AES-GCM encapsulé dans TCP. Les tests contrôlent le framing exact, les refus de paramètres ou capacités invalides et le déchiffrement octet pour octet côté serveur simulé.

Référence : [aos281_288_http_post_json.md](aos281_288_http_post_json.md). L’implémentation n’assure pas encore la fragmentation de très grandes requêtes, chunked, HTTP/2, authentification HTTP/API, retries, streaming LLM, handshake de production, hostname, chaîne X.509, dates ni X25519 constante-temps.


### AOS-289 à AOS-296 — validation hostname X.509/TLS

Le parseur X.509 publie le `commonName` et les `subjectAltName` DNS, puis `x509_certificate_hostname_validate` applique une comparaison DNS ASCII bornée. Les dNSName SAN sont prioritaires sur le CN, la casse ASCII est ignorée, et le wildcard `*.suffix` ne couvre qu’un seul label. Les tests couvrent le CN, la priorité SAN, le fallback sans SAN, la casse et le rejet d’un wildcard multi-label.

Référence : [aos289_296_tls_hostname.md](aos289_296_tls_hostname.md). Les IP, IDNA, Unicode, contraintes de nom, usages de clé, chaîne de confiance, ancres, dates, signatures de certificats et l’invocation automatique depuis le pilote restent à implémenter ; X25519/bigint demeure non constante-temps.


### AOS-297 à AOS-304 — chaîne X.509 RSA minimale à une ancre

`x509_certificate_chain_validate_one` valide désormais une feuille directement émise par une ancre X.509 caller-owned. Il impose l’égalité `issuer`/`subject`, valide la clé RSA de l’ancre, hash le DER complet TBSCertificate avec SHA-256 et vérifie la signature `sha256WithRSAEncryption` via RSA PKCS#1 v1.5 avec workspace fourni. Les vues TBS DER, algorithme et signature sont publiées par le parseur.

Le test utilise une racine RSA 1024 bits et une feuille RSA/SHA-256 signée, intégrées comme vecteur DER hors ligne ; il couvre le succès, un émetteur d’ancre altéré et une signature tronquée. Référence : [aos297_304_x509_trust_chain.md](aos297_304_x509_trust_chain.md).

Les intermédiaires, dates, contraintes et usages de clé, révocation, ECDSA/EdDSA, algorithmes SHA-384+, configuration des ancres et invocation automatique depuis NE2000 restent absents. Le hostname demeure une validation séparée et X25519/bigint reste non constante-temps.


### AOS-305 à AOS-312 — orchestrateur handshake TLS depuis NE2000

`ne2k_tls_client_t` relie désormais le polling TCP NE2000 au réassembleur TLS authentifié, à la validation de chaîne RSA à une ancre et hostname, au flight X25519 et au post-flight AES-GCM. `ne2k_tls_client_start` émet ClientHello transactionnellement ; `ne2k_tls_client_poll` accumule les fragments, contrôle l’identité, émet le flight une fois `ServerHelloDone` reçu et marque le handshake complet après Finished serveur.

Le contexte et tous les buffers/workspaces sont caller-owned. Les tests NE2000 couvrent l’initialisation, l’émission ClientHello, les séquences/transcript et le polling vide sans mutation ; les tests TCP/TLS existants couvrent le flux cryptographique sous-jacent. Référence : [aos305_312_ne2k_tls_orchestrator.md](aos305_312_ne2k_tls_orchestrator.md).

Le flux ne constitue pas encore un HTTPS de production : un message serveur par record, pas de certificats clients, chaîne RSA directe sans intermédiaires/dates/usages/révocation/ECDSA, pas de test QEMU contre un serveur externe, ni résolution/connexion complète, HTTP LLM, auth API, retries ou fragmentation de grosses requêtes. X25519/bigint reste non constante-temps.


### AOS-313 à AOS-320 — client HTTPS LLM sur NE2000

`ne2k_https_llm_post_json` construit et chiffre un POST JSON HTTP/1.1 uniquement après le Finished TLS complet, puis l’émet via TCP/NE2000 avec rollback de la séquence TCP et du compteur AEAD en cas d’échec. `ne2k_https_llm_poll_response` ouvre les records AES-GCM reçus puis alimente l’accumulateur HTTP Content-Length caller-owned et ACK seulement après traitement réussi.

Le module HTTP/TLS est désormais lié au test NE2000 ; les tests HTTP/TLS couvrent le framing POST et le round-trip AES-GCM, tandis que le test NE2000 couvre la préparation TLS et le polling vide. Référence : [aos313_320_https_llm_client.md](aos313_320_https_llm_client.md).

Aucune clé API, auth Authorization, intégration spécifique OpenAI/Ollama, DNS/SYN de connexion, test QEMU contre serveur externe, streaming SSE, chunked, HTTP/2, retries, pagination, compression, grandes requêtes ni parsing JSON n’est encore livré. La PKI reste une ancre RSA directe et X25519/bigint reste non constante-temps.


### AOS-321 à AOS-328 — Authorization Bearer caller-owned

`net_http_build_post_json_bearer` ajoute `Authorization: Bearer <token>` à un POST JSON borné sans conserver le token dans les contextes TCP, TLS ou NE2000. Le framing impose un token non vide en ASCII imprimable, la capacité caller-owned et les mêmes règles HTTP Content-Length. Le test vérifie le plaintext exact, un token fictif, les bornes et les rejets.

Référence : [aos321_328_http_authorization.md](aos321_328_http_authorization.md). Le secret réel, son stockage/effacement, rotation, révocation, auth mutuelle, formats OpenAI/Ollama, proxy, HTTP/2 et streaming restent hors périmètre.


### AOS-329 à AOS-336 — HTTP Transfer-Encoding chunked

`net_http_chunked_accumulator_t` décode désormais des réponses HTTP `Transfer-Encoding: chunked` à travers plusieurs plaintexts TLS, avec tailles hexadécimales bornées, CRLF contrôlés et compactage du body dans le buffer caller-owned. La publication intervient uniquement après `0\r\n\r\n`. Le vecteur couvre `Wikipedia` réparti sur deux fragments et le rejet d’une taille `Z` invalide.

Référence : [aos329_336_http_chunked.md](aos329_336_http_chunked.md). Les extensions de chunk, trailers non vides, compression, HTTP/2, SSE, grands corps et le wrapper TLS/NE2000 chunked restent hors périmètre.


### AOS-337 à AOS-344 — validité temporelle X.509

`x509_certificate_valid_at` vérifie désormais `notBefore <= instant <= notAfter` à partir d’un instant UTC caller-owned `YYYYMMDDhhmmssZ`. Les dates ASN.1 UTCTime et GeneralizedTime sont contrôlées, y compris calendrier et années bissextiles. Aucune horloge implicite n’est utilisée.

Les tests couvrent les instants interne, prématuré, expiré et une date calendairement invalide. Référence : [aos337_344_x509_validity_dates.md](aos337_344_x509_validity_dates.md). RTC/NTP, synchronisation sécurisée, tolérance d’horloge et appel automatique depuis l’orchestrateur TLS restent à implémenter.


### AOS-345 à AOS-352 — politique TLS chaîne, hostname et date

`x509_certificate_tls_identity_validate` exige désormais simultanément la chaîne RSA directe caller-owned, le hostname DNS et l’instant UTC caller-owned. Le test couvre le succès sur l’ancre/feuille de référence, le rejet d’un nom incorrect et d’une date prématurée.

Référence : [aos345_352_tls_trust_policy.md](aos345_352_tls_trust_policy.md). L’orchestrateur NE2000 ne fournit pas encore l’instant UTC à la politique. Intermédiaires, contraintes/usages, révocation, ECDSA et RTC/NTP sécurisé restent absents.


### AOS-353 à AOS-360 — politique temporelle dans NE2000/TLS

`ne2k_tls_client_poll` reçoit un instant UTC caller-owned et applique maintenant `x509_certificate_tls_identity_validate` avant de marquer le pair valide ou d’émettre le flight X25519. Toute erreur de chaîne, hostname ou date déclenche le rollback existant. Le test NE2000 fournit explicitement l’instant UTC du nouveau contrat ; les tests X.509 couvrent la politique de confiance.

Référence : [aos353_360_ne2k_tls_time_policy.md](aos353_360_ne2k_tls_time_policy.md). RTC/NTP sécurisé, handshake complet QEMU contre serveur externe, intermédiaires, révocation et ECDSA restent à implémenter.


### AOS-361 à AOS-368 — extraction JSON de réponses LLM

`net_json_extract_string` extrait et décode une valeur JSON string pour une clé ASCII dans un buffer caller-owned. Les échappements JSON simples sont décodés ; clé absente, contrôle brut, Unicode `\uXXXX`, string incomplète et capacité insuffisante sont rejetés. Le test couvre le champ `response`, le saut de ligne, les limites et le rejet Unicode.

Référence : [aos361_368_llm_json_extractor.md](aos361_368_llm_json_extractor.md). Ce n’est pas un parseur JSON complet : Unicode, tableaux, objets structurés, types non-string et SSE restent hors périmètre, de même que les adaptateurs OpenAI/Ollama spécifiques.


### AOS-369 à AOS-376 — adaptateurs de réponse Ollama/OpenAI

`net_llm_ollama_response_extract` extrait le champ `response` d’Ollama non-streaming ; `net_llm_openai_response_extract` extrait le premier champ string `content` d’une réponse OpenAI compatible. Les adaptateurs restent caller-owned et propagent les rejets de l’extracteur JSON. Les tests couvrent les deux formats simples et les champs fournisseur absents.

Référence : [aos369_376_llm_response_adapters.md](aos369_376_llm_response_adapters.md). L’adaptateur OpenAI ne valide pas la structure `choices[0].message.content`; SSE, tool calls, multi-choix, Unicode, médias et variations de schéma restent hors périmètre.


### AOS-377 à AOS-384 — builders JSON de requêtes LLM

`net_llm_build_ollama_generate_json` produit le body Ollama non-streaming `model`/`prompt`; `net_llm_build_openai_chat_json` produit le body OpenAI compatible avec un message utilisateur. Les chaînes caller-owned échappent guillemets, antislashs et espaces de contrôle courants ; contrôles non gérés, octets non ASCII et capacités insuffisantes sont rejetés.

Les tests vérifient les bodies exacts avec guillemets/saut de ligne et les rejets. Référence : [aos377_384_llm_request_builders.md](aos377_384_llm_request_builders.md). Header Bearer par fournisseur, Unicode, paramètres de génération, multi-tours, tool calls, média et SSE restent hors périmètre.
