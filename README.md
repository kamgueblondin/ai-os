# AI-OS - Système d'Exploitation pour Intelligence Artificielle

[![Version](https://img.shields.io/badge/version-6.0-blue.svg)](https://github.com/kamgueblondin/ai-os)
[![Status](https://img.shields.io/badge/status-stable-green.svg)](https://github.com/kamgueblondin/ai-os)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

## 🎯 Description

AI-OS est un système d'exploitation spécialement conçu pour héberger et exécuter des applications d'intelligence artificielle. Il offre une architecture modulaire optimisée pour les charges de travail IA avec un environnement sécurisé et performant.

## ⭐ Fonctionnalités Principales

- **🖥️ Shell Interactif Complet** - Interface utilisateur entièrement fonctionnelle
- **🤖 Simulateur d'IA Intégré** - Intelligence artificielle simulée avec base de connaissances
- **🛡️ Espace Utilisateur Sécurisé** - Isolation complète Ring 0/3 avec protection mémoire
- **⚡ Multitâche Préemptif** - Ordonnanceur Round-Robin avec changement de contexte optimisé
- **💾 Système de Fichiers** - Support Initrd avec parser TAR POSIX
- **🧠 Gestion Mémoire Avancée** - VMM/PMM avec paging complet (support ~128MB RAM)
- **🔌 Gestion Interruptions** - PIC, clavier, timer avec handlers optimisés

## 🚀 Démarrage Rapide

### Prérequis
```bash
sudo apt-get install build-essential nasm qemu-system-i386
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

### ✅ Problème du Clavier Résolu

Le problème critique où le clavier ne réagissait pas dans l'espace utilisateur a été **entièrement résolu**. 

#### Corrections Apportées :

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

## 📚 Documentation

### Guides Techniques
- [`docs/README.md`](docs/README.md) - Documentation complète du projet
- [`docs/etape_7_shell_ia.md`](docs/etape_7_shell_ia.md) - Documentation Shell & IA
- [`CORRECTION_CLAVIER_FINALE.md`](CORRECTION_CLAVIER_FINALE.md) - Détail des corrections v6.0

### Rapports de Développement
- [`RAPPORT_CORRECTION_FINALE.md`](RAPPORT_CORRECTION_FINALE.md) - Rapport final des corrections
- [`ANALYSE_ARCHITECTURE.md`](ANALYSE_ARCHITECTURE.md) - Analyse architecture système
- [`DIAGNOSTIC_COMPLET.md`](DIAGNOSTIC_COMPLET.md) - Diagnostic technique complet

## 🛣️ Roadmap

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

## 📞 Support

- **Repository** : [github.com/kamgueblondin/ai-os](https://github.com/kamgueblondin/ai-os)
- **Issues** : Rapports de bugs et demandes de fonctionnalités
- **Documentation** : Dossier `docs/` pour guides détaillés

---

**AI-OS v6.0** - *Système d'exploitation multitâche avec shell interactif fonctionnel*  
*Prêt pour l'hébergement d'intelligence artificielle* 🤖

**Développé avec ❤️ pour l'avenir de l'IA**

*Dernière mise à jour : 2025-08-27*
