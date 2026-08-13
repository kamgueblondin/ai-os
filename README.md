# AI-OS - Système d'Exploitation pour Intelligence Artificielle

[![Version](https://img.shields.io/badge/version-6.1-blue.svg)](https://github.com/kamgueblondin/ai-os)
[![Status](https://img.shields.io/badge/status-prototype-yellow.svg)](https://github.com/kamgueblondin/ai-os)
[![Keyboard](https://img.shields.io/badge/keyboard-FIXED-brightgreen.svg)](https://github.com/kamgueblondin/ai-os)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

## 🎯 Description

AI-OS est un **prototype de noyau pédagogique i386 32-bit** (hobby OS) qui boote sous QEMU, isole Ring 0/3 et lance un shell utilisateur ELF. Le noyau charge un initrd TAR, fournit un overlay RAM non persistant et expose une ABI de syscalls au shell utilisateur.

**État réel du code (août 2026) :** [docs/ETAT_REEL.md](docs/ETAT_REEL.md) — source de vérité fonctionnelle. L’index de la documentation se trouve dans [docs/README.md](docs/README.md).

## Mise à jour v7 — GPT-2 local bare-metal

AI-OS dispose maintenant d’un **moteur d’inférence GPT-2 124M freestanding**. Quand le checkpoint `llm.c v3` et le tokenizer binaire sont placés localement dans `models/` avant le build, ils sont copiés dans l’initrd ; l’OS peut alors générer localement sans réseau, Ollama, API externe ni système hôte après démarrage.

| Composant | Implémentation actuelle |
|---|---|
| Exécution locale | GPT-2 124M `.bin`, tokenizer binaire, syscall `SYS_GPT2_GENERATE` |
| Attention | Cache clé/valeur persistant lors de la génération d’un même préfixe |
| Échantillonnage | Top-k, pénalité de répétition et générateur pseudo-aléatoire |
| Optimisation | `-O3`, SSE2, `-mfpmath=sse`, `-mstackrealign` et `-fomit-frame-pointer` |
| Ressources | CPU SSE2 et 1 Gio de RAM QEMU requis pour le checkpoint de référence |
| Réseau | Profil `openai` sélectionnable dans le shell, mais Ethernet, TCP/IP, DNS et TLS restent à implémenter |

Les fichiers de modèle ne sont **pas versionnés** : ils sont volumineux et doivent être fournis par le constructeur de l’image. Pour une image ISO autonome sur une machine vierge, créez le répertoire suivant, puis construisez avec `make iso`.

```text
models/
├── gpt2_124M.bin
└── gpt2_tokenizer.bin
```

Dans le shell, utilisez `ai hello` pour une génération locale, `ai-provider local` pour sélectionner le moteur embarqué, `ai-model list` pour afficher les profils, `ai-runtime` pour les limites d’exécution et `rc` pour confirmer la reprise du shell après une réponse.

> Le cache KV, SSE2 et le réalignement de pile ont réduit la latence observée de `88,835 s` à **`7,693 s`** pour `ai hello` et quatre jetons sous QEMU Pentium III sans KVM. L’objectif inférieur à une seconde n’est donc pas atteint dans cet environnement ; il requerra une exécution native ou KVM, des poids quantifiés et des kernels SIMD plus avancés. Voir [docs/kv_cache_performance_report.md](docs/kv_cache_performance_report.md).

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

- **🖥️ Shell Interactif** - Prompt `/ (-.-) :` en Ring 3. `ls`/`cat` lisent l’initrd + overlay noyau ; `mkdir`/`rm`/`cp`/`mv`/`touch`/`write`/`append` mutent l’overlay (pas de disque persistant). `ps`/`kill`/`getpid`/`mem`/`uptime` interrogent le noyau.
- **🤖 GPT-2 Local Optionnel** - `ai <texte>` utilise `SYS_GPT2_GENERATE` avec un checkpoint embarqué lorsqu’il est disponible ; à défaut, le shell affiche un état d’indisponibilité et conserve les utilitaires historiques.
- **🛡️ Espace Utilisateur Sécurisé** - Isolation Ring 0/3, chargeur ELF, syscalls
- **⚡ Tâches et changement de contexte** - Passage kernel → shell via `jump_to_task()` ; le round-robin à chaque tick n’est pas le mode actuel (stabilité)
- **💾 Système de Fichiers** - Initrd TAR en lecture seule + overlay RAM (`mkdir`/`rm`/`cp`/`mv` fichier et dossier). Pas de disque persistant. `ls` fusionne les deux via `SYS_LISTDIR`.
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

AI-OS inclut une suite Unity de tests unitaires (kernel + userspace). En août 2026 : **121 tests** répartis dans `test_pmm` (17), `test_syscall` (48), `test_task` (21), `test_shell` (25) et `test_ramfs` (10). `make test-all` est la commande de référence et ne requiert pas les poids du modèle. Les tests QEMU de GPT-2 sont séparés : `make gpt2-recovery` vérifie qu’une commande `rc` est exécutée après une génération réelle ; `make gpt2-benchmark` mesure la latence avec le CPU virtuel SSE2 ; `make gpt2-tests` lance les deux. Ils requièrent les fichiers sous `models/`.

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
│   ├── fake_ai.c         # Programme historique de compatibilité
│   └── ../kernel/llm/    # Chargeur, tokenizer et inférence GPT-2 freestanding
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
- ✅ **Commandes de `help` branchées** (`mkdir`, `ls`, `cp`, `grep`, `kill`, `top`, `ai`, etc. — `ls`/`mkdir`/`rm`/`cp`/`mv` (fichiers et dossiers overlay)/`ps`/`kill`/`mem` via syscalls noyau, voir `docs/ETAT_REEL.md`)

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

Le dossier [`US/`](US/README.md) détaille une vision plus large (MOHHOS, 8 phases) : ces documents sont des spécifications, non l’état du code.

### Version 7.0 - IA locale bare-metal
- [x] Moteur d’inférence GPT-2 intégré
- [x] Cache KV et vectorisation SSE2
- [x] Sélecteur de fournisseur et de profil de modèle dans le shell
- [ ] Tokenizer BPE complet, génération longue et format quantifié
- [ ] Chargeur GGUF exécutable
- [ ] Réseau, DNS, TLS et fournisseur OpenAI effectif

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

**AI-OS v7 (branche GPT-2)** — *Prototype de noyau i386 avec shell userspace et inférence GPT-2 locale optionnelle sous QEMU.*
*Le modèle est intégré à l’ISO seulement lorsqu’il est fourni localement lors de la compilation.*

**Développé avec ❤️ pour l'avenir de l'IA**

*Dernière mise à jour : 2026-08-13 — documentation alignée sur l’état réel du code*
