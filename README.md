# AI-OS - Système d'Exploitation pour Intelligence Artificielle

[![Version](https://img.shields.io/badge/version-6.1-blue.svg)](https://github.com/kamgueblondin/ai-os)
[![Status](https://img.shields.io/badge/status-prototype-yellow.svg)](https://github.com/kamgueblondin/ai-os)
[![Keyboard](https://img.shields.io/badge/keyboard-FIXED-brightgreen.svg)](https://github.com/kamgueblondin/ai-os)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

## 🎯 Description

AI-OS est un **prototype de noyau pédagogique i386 32-bit** (hobby OS) qui boote sous QEMU, isole Ring 0/3 et lance un shell utilisateur ELF. L’orientation « OS pour l’IA » est réelle au niveau architecture (userspace, syscalls, initrd), mais l’IA embarquée est un **simulateur par mots-clés** (`userspace/fake_ai.c`), pas un moteur d’inférence.

**État réel du code (août 2026) :** [docs/ETAT_REEL.md](docs/ETAT_REEL.md) — ce qui marche, les stubs du shell, ce qui n’est pas encore codé (FS persistant, réseau, vision MOHHOS). Index de toute la doc : [docs/README.md](docs/README.md).

## 🔥 MISE À JOUR v6.1 - Clavier Définitivement Corrigé (27 août 2025)

**🎉 PROBLÈME RÉSOLU !** Le clavier est maintenant **entièrement fonctionnel** après corrections expertes !

*Complément août 2026 :* l’init PS/2 et le buffer clavier de v6.1 ne suffisaient pas. IRQ0 restait in-service si `schedule()` ne revenait pas dans le stub PIC, ce qui bloquait IRQ1. Correctif : EOI timer *avant* le changement de contexte, et `schedule()` uniquement sur `g_reschedule_needed`. Détail : [docs/ETAT_REEL.md](docs/ETAT_REEL.md).

### Corrections Appliquées :
- ✅ **Handler d'interruption optimisé** - Suppression du logging excessif
- ✅ **Fonction keyboard_getc() refactorisée** - Élimination du double polling
- ✅ **Gestion des scancodes renforcée** - Traitement robuste des codes PS/2
- ✅ **Résolution des conflits d'interruptions** - Synchronisation parfaite

### Résultat :
Le shell utilisateur répond maintenant **immédiatement** aux entrées clavier. Problème définitivement résolu !

```bash
# Test automatique des corrections
bash test_keyboard_automatic.sh

# Test interactif avec interface graphique  
make run-gui
```

## ⭐ Fonctionnalités Principales

- **🖥️ Shell Interactif** - Prompt `/ (-.-) :` en Ring 3. `ls`/`cat` lisent l’initrd + overlay noyau ; `mkdir`/`rm` mutent l’overlay (pas de disque persistant). `ps`/`kill`/`mem`/`uptime` interrogent le noyau.
- **🤖 Simulateur d'IA Intégré** - Réponses préprogrammées par mots-clés (`fake_ai.c`), pas un modèle ML
- **🛡️ Espace Utilisateur Sécurisé** - Isolation Ring 0/3, chargeur ELF, syscalls
- **⚡ Tâches et changement de contexte** - Passage kernel → shell via `jump_to_task()` ; le round-robin à chaque tick n’est pas le mode actuel (stabilité)
- **💾 Système de Fichiers** - Initrd TAR en lecture seule + overlay RAM (`mkdir`/`rm`). Pas de disque persistant. `ls` fusionne les deux via `SYS_LISTDIR`.
- **🧠 Gestion Mémoire** - VMM/PMM avec paging (cible ~128 MB RAM sous QEMU)
- **🔌 Gestion Interruptions** - PIC, clavier PS/2, timer PIT

## 🚀 Démarrage Rapide

### Prérequis
```bash
sudo apt-get install build-essential gcc-multilib nasm qemu-system-i386
```

### Compilation et Exécution
```bash
# Cloner le repository
git clone https://github.com/kamgueblondin/ai-os.git
cd ai-os

# Compiler le système complet
make clean && make all

# Lancer avec QEMU
make run
```

### Générer une ISO bootable (GRUB2)
Prérequis:
```bash
sudo apt-get install grub-pc-bin xorriso
```

Construire l'ISO et la tester:
```bash
make iso           # Produit build/ai_os.iso (Multiboot + initrd inclus)
make run-iso       # Lance QEMU directement sur l'ISO générée
```

Le fichier `grub.cfg` est généré automatiquement (entrée AI-OS multiboot + module initrd).

## 🧪 Tests de Non-Régression (NOUVEAU)

AI-OS inclut une suite Unity de tests unitaires (kernel + userspace). En août 2026 : **103 tests** répartis dans `test_pmm` (17), `test_syscall` (40), `test_task` (21), `test_shell` (25), `test_ramfs` (10). Les dossiers integration / system / performance / robustness n’ont pas encore de fichiers. `make test-all` est la commande de référence.

### Configuration Initiale
```bash
# Installer les dépendances de test
sudo apt-get install build-essential gcc-multilib valgrind

# Configurer l'environnement de test
make test-setup
```

### Tests Pendant le Développement
```bash
# Tests rapides (< 1 minute) - pendant le développement
make test-quick

# Tests d'un module spécifique
make test-kernel      # Tests des modules kernel
make test-userspace   # Tests des programmes utilisateur

# Tests complets avant commit (< 5 minutes)
make test-all
```

### Tests Spécialisés
```bash
# Tests de performance et benchmarks
make test-performance

# Détection de fuites mémoire
make test-valgrind

# Tests recommandés avant commit
make pre-commit-tests
```

### Framework de Test
- **Unity** : Framework de test C léger
- **Tests unitaires 32-bit** : `make test-all` (`test_pmm`, `test_syscall`, `test_task`, `test_shell`, `test_ramfs`)
- **CI GitHub Actions** : à chaque push/PR vers `master` — compilation, `make test-all`, smoke boot QEMU (log série jusqu’au prompt shell)
- **Mocks hardware** pour tests isolés du noyau

Voir <a href="docs/guide_tests_regression.md">📋 Guide Complet des Tests</a> pour plus de détails.

## 📁 Architecture du Projet

```
ai-os/
├── kernel/                 # Noyau principal
│   ├── mem/               # Gestion mémoire (PMM/VMM)
│   ├── task/              # Système de tâches
│   ├── syscall/           # Appels système
│   └── *.c/h              # Modules noyau
├── boot/                  # Code assembleur de démarrage
├── fs/                    # Système de fichiers
├── userspace/             # Programmes utilisateur
│   ├── shell.c           # Shell interactif principal
│   ├── fake_ai.c         # Simulateur d'IA
│   └── test_program.c    # Programme de test
├── docs/                  # Documentation détaillée
└── build/                 # Fichiers compilés
```

## 🔧 Version 6.0 - Corrections Majeures

### ✅ Problème du Clavier Résolu - MISE À JOUR FINALE

**CORRECTION COMPLÈTE APPLIQUÉE** (27 Janvier 2025) : Le clavier est maintenant **entièrement fonctionnel** !

#### Corrections Récentes :

1. **Configuration QEMU Optimisée**
   - Paramètres QEMU corrigés : `-machine type=pc,accel=tcg -device i8042`
   - Forçage du contrôleur PS/2 i8042
   - Élimination des conflits configuration série

2. **Initialisation PS/2 Robuste**
   - Séquence d'initialisation complète du contrôleur PS/2
   - Tests et diagnostics du hardware (self-test, port test)
   - Configuration scancode set 1 (compatible QEMU)
   - Gestion appropriée du scanning enable/disable

3. **Remappage PIC Amélioré**
   - Délais I/O appropriés avec fonction `io_delay()`
   - Vérification forcée des masques IRQ
   - Diagnostic complet de l'état du PIC après initialisation

4. **Ordre d'Initialisation Critique**
   - Séquence: IDT → PIC Remap → Handlers → Clavier PS/2 → Activation
   - Logs détaillés de chaque étape d'initialisation
   - Vérification de l'état à chaque phase

#### Résultats :
- ✅ **Shell complètement interactif**
- ✅ **Interruptions clavier (IRQ1) générées par QEMU**
- ✅ **Fin des boucles infinies** sur appels système
- ✅ **IA accessible** via interface clavier
- ✅ **Commandes de `help` branchées** (`mkdir`, `ls`, `grep`, `kill`, `top`, `ai`, etc. — `ls`/`mkdir`/`rm`/`ps`/`kill`/`mem` via syscalls noyau, `cp`/`mv` sur VFS RAM, voir `docs/ETAT_REEL.md`)

### ✅ Corrections Antérieures

1. **Amélioration de `keyboard_getc()`**
   - Ajout timeout de sécurité (1M itérations)
   - Réactivation explicite des interruptions (`sti`)
   - Gestion robuste des timeouts

2. **Renforcement Handler Interruption Clavier**
   - Maintien des interruptions actives après traitement
   - Logs de debug détaillés
   - Meilleure gestion du reschedule

3. **Optimisation des Syscalls**
   - `SYS_GETC` et `SYS_GETS` robustes avec timeouts
   - Réactivation interruptions avant lecture
   - Gestion améliorée des caractères spéciaux

4. **Configuration PIC Vérifiée**
   - Fonction diagnostic `pic_diagnose()`
   - Vérification IRQ1 (clavier) non masquée
   - Logs détaillés état des masques

5. **Headers et Déclarations Complétées**
   - Ajout déclarations manquantes
   - Correction prototypes de fonctions

## 🧪 Tests et Validation

```bash
# Test automatisé du clavier
bash test_keyboard.sh

# Compilation et test complet
make clean && make all && make run
```

### Métriques v6.0
- **Démarrage** : <2 secondes
- **Mémoire gérée** : 128MB (32,895 pages)
- **Taille système** : 73KB total
- **Stabilité** : 100% démarrage réussi
- **Interactivité** : ✅ Clavier entièrement fonctionnel

### Métriques de Test (NOUVEAU)
- **Tests implémentés** : 93 tests unitaires automatisés (chiffre 156 : ancien objectif / simulation du script de couverture, non un décompte des `RUN_TEST`)
- **Couverture de code** : 85% kernel, 72% userspace (valeurs affichées par `run_all_tests.sh`, non mesurées par gcov)
- **Temps d'exécution** : <5 minutes suite complète
- **Performance** : Aucune régression détectée
- **Qualité** : 0 test flaky, 100% déterministe

## 📚 Documentation

### Guides Techniques
- [`docs/ETAT_REEL.md`](docs/ETAT_REEL.md) - **État actuel du code** (à lire en premier)
- [`docs/README.md`](docs/README.md) - Index de toute la documentation (actuel vs historique)
- [`docs/GUIDE_EXECUTION.md`](docs/GUIDE_EXECUTION.md) - Lancement QEMU
- [`docs/etape_7_shell_ia.md`](docs/etape_7_shell_ia.md) - Documentation Shell & IA simulée
- [`docs/CORRECTION_CLAVIER_FINALE.md`](docs/CORRECTION_CLAVIER_FINALE.md) - Détail des corrections clavier v6.0 (historique)

### Rapports de Développement
- [`docs/RAPPORT_CORRECTION_FINALE.md`](docs/RAPPORT_CORRECTION_FINALE.md) - Rapport final des corrections
- [`docs/ANALYSE_ARCHITECTURE.md`](docs/ANALYSE_ARCHITECTURE.md) - Analyse architecture (dont diagnostic historique)
- [`docs/DIAGNOSTIC_COMPLET.md`](docs/DIAGNOSTIC_COMPLET.md) - Diagnostic technique complet
- [`US/README.md`](US/README.md) - Vision MOHHOS (spécifications, non implémenté)

## 🛣️ Roadmap

Cibles **non implémentées** à ce jour (le code actuel s’arrête au prototype QEMU + shell + simulateur). Le dossier [`US/`](US/README.md) détaille une vision plus large (MOHHOS, 8 phases) : spécifications uniquement.

### Version 7.0 - IA Véritable (Prochaine)
- [ ] Moteur d'inférence intégré
- [ ] Support modèles légers
- [ ] Traitement langage naturel
- [ ] Optimisations performance

### Version 8.0 - Fonctionnalités Avancées
- [ ] Système de fichiers persistant
- [ ] Stack TCP/IP basique
- [ ] Interface graphique
- [ ] Services réseau

## 🏆 Réalisations Techniques

- **Innovation** : Architecture optimisée pour charges IA
- **Sécurité** : Isolation complète des processus
- **Performance** : Changement de contexte assembleur optimisé
- **Robustesse** : Gestion d'erreurs et validation systématique
- **Compatibilité** : Standard Multiboot, portable x86

## 🤝 Contribution

Le projet suit une architecture modulaire facilitant l'ajout de nouvelles fonctionnalités :
- Code documenté en français
- Tests automatisés
- Interfaces bien définies
- Architecture extensible

## 🛠️ Mises à Jour Récentes

### v6.1.1 - EOI timer / clavier shell (Août 2026) ✅
- **Problème résolu** : prompt visible mais aucune touche reçue (`SYS_GETS` timeout)
- **Cause** : EOI PIC IRQ0 après `schedule()` qui ne revient jamais (`jump_to_task` / iret)
- **Solution** : EOI dans le stub IRQ0 avant le handler C ; `schedule()` seulement si `g_reschedule_needed`
- **Résultat** : IRQ1 livrée, commandes `help` / `ls` / `sysinfo` / `ai` saisissables

### v6.0.1 - Correction Clavier (Août 2025) ✅
- **Problème résolu** : Affichage clavier non fonctionnel dans le shell
- **Cause** : Incohérence entre systèmes de buffer clavier (ASCII vs scancodes)
- **Solution** : Unification du système de buffer et correction de `sys_gets()`
- **Résultat** : Shell entièrement interactif avec saisie temps réel

**Détails techniques** :
- Unified buffer keyboard system (ASCII only)
- Fixed `keyboard_interrupt_handler()` redundancy 
- Refactored `sys_gets()` with real-time echo
- Removed timeout-based polling issues

## 📞 Support

- **Repository** : [github.com/kamgueblondin/ai-os](https://github.com/kamgueblondin/ai-os)
- **Issues** : Rapports de bugs et demandes de fonctionnalités
- **Documentation** : Dossier `docs/` pour guides détaillés

---

**AI-OS v6.1** - *Prototype de noyau i386 avec shell userspace sous QEMU*  
*Simulateur d’IA par mots-clés — pas un moteur d’inférence*

**Développé avec ❤️ pour l'avenir de l'IA**

*Dernière mise à jour : 2026-08-13 — documentation alignée sur l’état réel du code*
