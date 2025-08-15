# AI-OS - Système d'Exploitation pour Intelligence Artificielle

## 🎯 Vision du Projet

AI-OS est un système d'exploitation spécialement conçu pour héberger et exécuter des applications d'intelligence artificielle de manière sécurisée et efficace. Le projet vise à créer une plateforme optimisée pour les charges de travail IA avec une architecture modulaire et extensible.

## 🚀 Fonctionnalités Actuelles (v4.0)

### 🧠 Gestion Avancée de la Mémoire
- **Physical Memory Manager (PMM)** : Gestion dynamique avec bitmap
- **Virtual Memory Manager (VMM)** : Paging complet avec protection
- **Isolation mémoire** : Sécurité entre processus
- **Support ~128MB RAM** : Détection automatique via Multiboot

### 💾 Système de Fichiers
- **Initrd avec parser TAR** : Système de fichiers en mémoire
- **Support POSIX** : Validation checksums et métadonnées
- **API complète** : Lecture, listage, gestion des fichiers
- **Chargement automatique** : Intégration Multiboot transparente

### ⚡ Multitâche Préemptif
- **Ordonnanceur Round-Robin** : Équitable et performant
- **Changement de contexte** : Optimisé en assembleur
- **Timer système (PIT)** : 100Hz pour réactivité
- **États de tâches** : RUNNING, READY, WAITING, TERMINATED

### 🛡️ Espace Utilisateur Sécurisé
- **Séparation Ring 0/3** : Isolation kernel/user complète
- **Appels système** : Interface sécurisée (5 syscalls)
- **Chargeur ELF** : Exécution de programmes externes
- **Protection mémoire** : Prévention des accès non autorisés

### 🔧 Infrastructure Système
- **Gestion des interruptions** : PIC, clavier, timer
- **Support Multiboot** : Récupération mémoire et modules
- **Debug dual** : Sortie VGA + série
- **Build system avancé** : Compilation automatisée

## 📁 Architecture du Projet

```
ai-os/
├── kernel/                   # Noyau principal
│   ├── mem/                 # Gestion mémoire (PMM/VMM)
│   ├── task/                # Système de tâches
│   ├── syscall/             # Appels système
│   ├── *.c/h                # Modules noyau (interruptions, timer, etc.)
├── boot/                    # Code assembleur de démarrage
│   ├── boot.s              # Point d'entrée Multiboot
│   ├── context_switch.s    # Changement de contexte
│   ├── paging.s            # Fonctions paging
│   └── *.s                 # Autres routines assembleur
├── fs/                      # Système de fichiers
│   ├── initrd.h/c          # Parser TAR pour initrd
├── userspace/               # Programmes utilisateur
│   ├── test_program.c      # Programme de démonstration
│   └── Makefile            # Build system utilisateur
├── docs/                    # Documentation complète
│   ├── README.md           # Ce fichier
│   ├── etapes_*_*.md       # Documentation détaillée par étape
└── build/                   # Fichiers compilés
```

## 🔨 Compilation et Exécution

### Prérequis
```bash
sudo apt-get install build-essential nasm qemu-system-i386
```

### Compilation Complète
```bash
make clean && make all
```

### Exécution avec QEMU
```bash
# Mode texte avec log série
make run

# Mode graphique
make run-gui

# Test de compilation uniquement
make test-build
```

### Compilation du Programme Utilisateur
```bash
# Compile seulement le programme utilisateur
make user-program

# Affiche les informations ELF
make info-user
```

### Gestion de l'Initrd
```bash
# Affiche le contenu de l'initrd
make info-initrd

# L'initrd est créé automatiquement avec :
# - Fichiers de test
# - Programme utilisateur compilé
# - Configuration système
```

## 🧪 Tests et Validation

### Tests Automatisés
- **Compilation** : Tous les modules compilent sans erreur
- **Démarrage** : Initialisation complète du système
- **Multitâche** : Tâches kernel s'exécutent en parallèle
- **Mémoire** : Allocation/libération de pages
- **Syscalls** : Appels système fonctionnels

### Démonstrations Visuelles
- **Coin inférieur droit** : Tâches A, B, C clignotent
- **Messages série** : Log détaillé des opérations
- **Programme utilisateur** : Exécution en Ring 3

### Métriques de Performance
- **Démarrage** : <2 secondes
- **Mémoire gérée** : 32,895 pages (128MB)
- **Fréquence timer** : 100Hz (10ms quantum)
- **Taille noyau** : ~20KB optimisé

## 📊 Évolution du Projet

### Version 1.0 - Noyau Basique
- Démarrage Multiboot
- Affichage VGA simple
- Structure de base

### Version 2.0 - Gestion Mémoire
- Physical Memory Manager
- Virtual Memory Manager avec paging
- Système de fichiers initrd
- Support TAR POSIX

### Version 3.0 - Interruptions
- Gestion complète des interruptions
- Support clavier
- PIC et IDT configurés

### Version 4.0 - Multitâche et Espace Utilisateur ⭐
- Système de tâches complet
- Ordonnanceur préemptif
- Appels système sécurisés
- Chargeur ELF
- Programmes utilisateur

## 🎯 Prochaines Étapes

### Version 5.0 - Shell et Interface
- Shell interactif complet
- Commandes système avancées
- Gestion des processus utilisateur
- Interface de configuration

### Version 6.0 - Réseau et Communication
- Stack TCP/IP basique
- Pilotes réseau
- Communication inter-processus
- Services réseau

### Version 7.0 - Intelligence Artificielle
- Moteur d'inférence intégré
- Support des modèles légers
- API IA pour applications
- Optimisations performance

## 🔧 Développement

### Structure de Développement
- **Langage principal** : C (noyau) + Assembleur (bas niveau)
- **Architecture cible** : x86 32-bit
- **Bootloader** : Multiboot compatible
- **Émulateur** : QEMU pour tests
- **Build system** : Make avec dépendances

### Bonnes Pratiques
- Code modulaire et documenté
- Tests automatisés à chaque étape
- Documentation technique complète
- Validation sur matériel réel possible

### Contribution
- Architecture extensible pour nouveaux modules
- Interfaces bien définies
- Code commenté en français
- Tests de régression

## 📚 Documentation

### Guides Techniques
- `docs/etapes_3_4_specifications.md` - Gestion mémoire et FS
- `docs/etapes_5_6_implementation.md` - Multitâche et espace utilisateur
- Code source entièrement commenté

### Ressources d'Apprentissage
- Architecture x86 et mode protégé
- Développement de systèmes d'exploitation
- Gestion de la mémoire virtuelle
- Programmation système avancée

## 🏆 Réalisations Techniques

### Innovation
- **Optimisé pour l'IA** : Architecture pensée pour les charges IA
- **Sécurité avancée** : Isolation complète des processus
- **Performance** : Changement de contexte optimisé
- **Modularité** : Ajout facile de nouvelles fonctionnalités

### Robustesse
- **Gestion d'erreurs** : Validation systématique
- **Stabilité** : Tests approfondis
- **Compatibilité** : Standard Multiboot
- **Portabilité** : Code x86 standard

## 📞 Support et Contact

### Repository GitHub
- **URL** : https://github.com/kamgueblondin/ai-os.git
- **Branches** : master (stable), dev (développement)
- **Issues** : Rapports de bugs et demandes de fonctionnalités
- **Wiki** : Documentation étendue

### Statut du Projet
- **Phase actuelle** : v4.0 - Multitâche et Espace Utilisateur
- **Stabilité** : Production-ready pour développement
- **Tests** : Validation complète sur QEMU
- **Roadmap** : Versions 5.0-7.0 planifiées

---

**AI-OS v4.0** - *Système d'exploitation multitâche avec espace utilisateur sécurisé*  
*Prêt pour l'hébergement d'intelligence artificielle* 🤖

**Développé avec ❤️ pour l'avenir de l'IA**

