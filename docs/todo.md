# TODO - Correction AI-OS Shell Utilisateur

> **État réel (août 2026).** Le shell utilisateur Ring 3 **se lance** et le clavier **répond** (correctif EOI IRQ0). Les cases des phases 5–6 ci-dessous sont mises à jour. Le diagnostic d’origine (phases 1–4) est **conservé** : il décrit correctement un état antérieur. Référence : [ETAT_REEL.md](ETAT_REEL.md).

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
1. **Transition Kernel→Userspace manquante**: Le système reste en mode simulation au lieu de passer au shell utilisateur
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
- ❌ **Mode simulation seulement** - Le système reste en mode kernel au lieu de passer au shell utilisateur *(constat d’alors)*
- 📝 **Point de blocage identifié**: Le code reste dans la boucle de simulation au lieu d'exécuter le shell utilisateur

*Mise à jour août 2026 :* le shell ELF userspace est lancé ; le « mode simulation seulement » ne s’applique plus. Voir phase 5.

## Phase 5: Correction et refactorisation ✅ (août 2026)
- [x] Corriger les problèmes identifiés (transition userspace via `jump_to_task`, EOI PIC avant `schedule()`)
- [x] Refactoriser le code si nécessaire (planification uniquement sur `g_reschedule_needed`)
- [x] Améliorer la stabilité du système (plus de reboot systématique après le timer)
- [x] Optimiser la communication kernel/userspace (syscalls GETS/EXEC fonctionnels)

### Reste ouvert (hors périmètre « shell qui démarre »)
- [x] Commandes listées dans `help` branchées dans `execute_builtin_command` (VFS RAM + table processus simulée)
- [x] `ls` / `cat` / `ps` / `kill` / `uptime` / `mem` : syscalls noyau (initrd + `task.c` + PIT + PMM)
- [x] Overlay noyau RAM : `mkdir` / `rm` / `cp` (fichier et dossier via `SYS_COPY`) / `mv` (fichier et dossier via `SYS_RENAME`) / `write` / `touch` / `echo >` visibles par `ls`/`cat` (initrd toujours read-only)
- [ ] FS persistant sur disque (l’overlay RAM n’est pas persisté)
- [ ] Préemption round-robin continue (aujourd’hui limitée pour la stabilité)
- [ ] Réseau, vrai moteur IA — voir roadmap README / dossier `US/`

## Phase 6: Tests finaux et soumission sur GitHub ✅ (août 2026)
- [x] Tests complets du système corrigé (`make test-all` : tests unitaires kernel + shell + ramfs)
- [x] Validation du fonctionnement en mode utilisateur (QEMU GTK + `sendkey`)
- [x] Commit et push des corrections sur GitHub
- [x] Documentation des corrections apportées ([ETAT_REEL.md](ETAT_REEL.md))

## Problème identifié (historique — phases 1 à 4)
Le mode simulation fonctionnait parfaitement mais le passage au mode utilisateur échoue - l'interface reste figée. Besoin d'analyser les appels système et la gestion des processus.

**Statut 2026 :** ce blocage est levé. Le kernel pose `g_reschedule_needed` puis le timer appelle `schedule()` une fois vers le shell ELF.

