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
- [x] Overlay persisté : snapshot AIOV V2 ATA PIO LBA28 sur disque IDE QEMU (`write` survit à un reboot) ; pas un volume FAT
- [x] `spawn` / `yield` coopératifs (cadre syscall user) et préemption IRQ0 sûre entre tâches Ring 3
- [x] `exec` bloquant : parent `TASK_WAITING`, enfant reveille via `SYS_EXIT` (plus de `int $0x30` noyau)
- [x] AOS-020…025 : sonde GGUF, BPE UTF-8, contrats QEMU, overlay V2, IRQ0, stub OpenAI
- [ ] Kernels GGUF Q3_K/Q4_K/Q6_K et latence locale &lt; 1 s
- [ ] Volume FAT sur IDE (AOS-026) — [aos_fat_volume.md](aos_fat_volume.md) ; pas ext2
- [ ] Pilote NIC, DHCP, DNS, TCP/TLS et client OpenAI effectif
- [x] Contrats QEMU dans `tests/integration` (cœur, IRQ0, fournisseur, IPC, VFS, services)

## Phase 6: Tests finaux et soumission sur GitHub ✅ (août 2026)
- [x] Tests complets du système corrigé (`make test-all` : 251 Unity ; `make qemu-smoke` et `make integration-qemu`)
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
| Ethernet, ARP, DHCP, DNS A | Implémentés et couverts par tests/smokes. |
| TCP SYN, réception, validation SYN-ACK, premier ACK en mémoire | Implémentés ; 295 tests verts sur AOS-150. |
| Émission physique de l’ACK via l’orchestrateur NE2000 | Prochain lot. |
| Segments TCP avec données caller-owned | Prochain lot. |
| Retransmissions, timers, congestion et fermeture complète | Non implémentés. |
| TLS et appels LLM en ligne | Non fonctionnels de bout en bout à ce stade. |

Cette section remplace toute interprétation de la ligne générique « Pilote NIC, DHCP, DNS, TCP/TLS et client OpenAI effectif » : les sous-composants réalisés sont précisés ci-dessus, tandis que TCP/TLS et le client LLM restent partiels.

Références détaillées : [AOS-149](aos149_tcp_synack_validation.md) et [AOS-150](aos150_tcp_first_ack.md).

### AOS-151 — émission du premier ACK via NE2000

AOS-151 est implémenté localement : `ne2k_tcp_ack` utilise le cache ARP caller-owned, construit Ethernet/IPv4/TCP, calcule les checksums et transmet via `ne2k_tx_submit`. Le test NE2000 valide les ports, séquences, ACK et adresses MAC. La validation complète et la PR restent à effectuer après la non-régression.

Le prochain lot logique est AOS-152 : codec TCP avec données caller-owned et émission bornée d’un segment ACK+payload. Cela ne constitue pas encore un client HTTP, TLS ou LLM fonctionnel.

### AOS-152 — données TCP caller-owned

AOS-152 ajoute le codec `ACK+payload` et `ne2k_tcp_data`, avec longueur IPv4 exacte, checksums TCP/IPv4 et contrôles de capacité. La suite locale atteint 297 tests verts. Le smoke `qemu-ai-provider` a échoué une première fois sur l’absence de sortie `Runtime IA bare-metal`, puis a réussi lors d’une relance immédiate ; le smoke `qemu-ne2k-status` est vert. Cette anomalie de smoke IA reste à surveiller séparément du chemin TCP.

### AOS-153/AOS-154 — séquences, ACK reçus et retransmission bornée

AOS-153 fait progresser explicitement `local_sequence` après envoi confirmé et `remote_sequence` après acceptation d’un payload en séquence. AOS-154 conserve une vue caller-owned du dernier payload et borne le nombre de retransmissions autorisées. Aucun timer, RTO, mécanisme de congestion ou stockage copié n’est introduit. La validation complète atteint désormais 299 tests verts, avec build i386 et deux smokes QEMU réussis.
