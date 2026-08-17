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
