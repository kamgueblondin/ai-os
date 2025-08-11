# AI-OS Project Documentation

This document provides an overview of the AI-OS project, detailing the initial setup, the core components, and the current development status.

## Project Overview

The AI-OS project aims to build a basic operating system. This initial phase focuses on setting up the development environment, creating the fundamental boot and kernel components, and enabling basic output functionality.

## Resources Used

The primary resource for this project is the detailed definition of files for "Étape 1: Le Socle - Démarrage et Noyau de Base" (Step 1: The Foundation - Boot and Basic Kernel). This document, originally provided as `pasted_content.txt`, outlines the necessary files and their content.

- `pasted_content.txt`: Original documentation detailing the project structure, file contents, and build instructions.

## Project Structure

The project follows the structure outlined in the `pasted_content.txt`:

```
projet-ai-os/
├── kernel/
│   └── kernel.c
├── boot/
│   └── boot.s
├── linker.ld
├── Makefile
└── .gitignore
```

## File Details and Modifications

### 1. `boot/boot.s`

This assembly file serves as the entry point for the operating system. It sets up the Multiboot header and the stack, then calls the `kmain` function in `kernel.c`.

**No modifications were made to this file.**

### 2. `kernel/kernel.c`

This C file contains the core of our kernel. Initially, it was designed to display a message directly to VGA memory. However, to enable output capture in the sandbox environment, modifications were made to redirect output to the serial port.

**Modifications:**
- Added `serial_init()`, `is_transmit_empty()`, `write_serial()`, `inb()`, and `outb()` functions for serial port communication.
- Modified `kmain()` to initialize the serial port and print the welcome message "Bienvenue dans AI-OS !" to the serial port instead of VGA.
- Renamed `print_char` to `print_char_vga` and `print_string` to `print_string_vga` to distinguish them from serial output functions.

### 3. `linker.ld`

This linker script instructs the `ld` tool on how to combine the compiled object files (`boot.o` and `kernel.o`) into the final OS executable. It defines the entry point and memory organization.

**No modifications were made to this file.**

### 4. `Makefile`

This Makefile automates the compilation, linking, and testing processes. It defines rules for building the OS image and running it with QEMU.

**Modifications:**
- The `run` target was modified to include `-nographic` and `-serial file:output.log` flags for QEMU. This allows the OS to run without a graphical interface and redirects all serial output to `output.log`, making it possible to capture the kernel's output in the sandbox environment.

### 5. `.gitignore`

This file specifies which files and directories Git should ignore, preventing generated files from being committed to the repository.

**No modifications were made to this file.**

## Current Development Status

As of now, the AI-OS project has successfully completed the initial setup and core component creation. The environment is configured, and the bootloader and kernel are compiled and linked. The OS can be run in QEMU, and its output is now redirected to a log file, confirming its basic functionality.

**Next Steps:**

1. **Further Kernel Development:** Implement more advanced features in the kernel, such as keyboard input, interrupt handling, and memory management.
2. **Driver Development:** Add drivers for various hardware components to enable more complex interactions.
3. **User Interaction:** Develop a basic shell or command-line interface for user interaction.

## How to Continue the Project

To continue working on this project, follow these steps:

1. **Clone the Repository:**
   ```bash
   git clone https://github.com/kamgueblondin/ai-os.git
   cd ai-os
   ```

2. **Install Prerequisites (if not already installed):**
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential nasm qemu-system-x86 git
   ```

3. **Build the OS:**
   ```bash
   make
   ```

4. **Run the OS in QEMU and view output:**
   ```bash
   make run
   cat output.log
   ```

This will run the AI-OS in QEMU, and the output will be displayed in your terminal from the `output.log` file. You can then proceed to modify `kernel.c` or other files to add new functionalities.



## Étapes 3 et 4 : Gestion de la Mémoire et Système de Fichiers

### Nouvelles Fonctionnalités Implémentées

#### Étape 3 : Gestion de la Mémoire (Memory Management)

La gestion de la mémoire a été ajoutée pour permettre l'allocation et la libération dynamique de mémoire, une fonctionnalité essentielle pour exécuter des programmes et gérer des données.

**Composants ajoutés :**

1. **Gestionnaire de Mémoire Physique (PMM - Physical Memory Manager)**
   - Fichiers : `kernel/mem/pmm.h`, `kernel/mem/pmm.c`
   - Fonction : Gestion des cadres de page de 4 Ko en RAM physique
   - Méthode : Utilisation d'un bitmap pour tracker les pages libres/utilisées

2. **Gestion de la Mémoire Virtuelle (VMM - Virtual Memory Manager)**
   - Fichiers : `kernel/mem/vmm.h`, `kernel/mem/vmm.c`
   - Fonction : Implémentation du paging pour la sécurité et la flexibilité
   - Méthode : Tables de pages pour la traduction adresses virtuelles → physiques

#### Étape 4 : Accès Disque et Système de Fichiers Primitif (Initrd)

Un système de fichiers basique a été implémenté utilisant un initrd (Initial RAM Disk) pour éviter la complexité d'un pilote de disque complet.

**Composants ajoutés :**

1. **Parser Initrd**
   - Fichiers : `fs/initrd.h`, `fs/initrd.c`
   - Format : Support du format TAR pour la simplicité
   - Fonction : Lecture de fichiers depuis un disque virtuel en RAM

### Structure Mise à Jour du Projet

```
projet-ai-os/
├── kernel/
│   ├── mem/                    # NOUVEAU DOSSIER
│   │   ├── pmm.c              # Physical Memory Manager
│   │   ├── pmm.h              
│   │   ├── vmm.c              # Virtual Memory Manager (Paging)
│   │   └── vmm.h              
│   ├── idt.h/idt.c            # Gestion de l'IDT
│   ├── interrupts.h/c         # Gestion du PIC et interruptions
│   ├── keyboard.h/c           # Pilote clavier
│   └── kernel.c               # Noyau principal
├── boot/
│   ├── boot.s                 # Point d'entrée
│   ├── idt_loader.s           # Chargement de l'IDT
│   ├── isr_stubs.s            # Stubs ISR
│   └── paging.s               # NOUVEAU: Fonctions paging en assembleur
├── fs/                        # NOUVEAU DOSSIER
│   ├── initrd.c               # Parser pour l'initrd (format TAR)
│   └── initrd.h               
├── docs/
│   ├── README.md              # Cette documentation
│   └── pasted_content.txt     # Spécifications originales
├── linker.ld                  # Script de liaison
├── Makefile                   # Makefile mis à jour
└── .gitignore
```

### Fonctionnalités Techniques

#### Gestion de la Mémoire

- **Allocation de pages** : Système de bitmap pour tracker les pages de 4 Ko
- **Mémoire virtuelle** : Mapping 1:1 pour les premiers 4 Mo de mémoire
- **Sécurité** : Isolation entre le noyau et les futurs programmes utilisateur
- **Registres CPU** : Utilisation de CR0 et CR3 pour activer le paging

#### Système de Fichiers Initrd

- **Format TAR** : Parser simple pour lire les fichiers archivés
- **Chargement** : L'initrd est chargé en RAM par QEMU au démarrage
- **API** : Fonctions pour lister et lire les fichiers contenus
- **Utilisation** : Base pour charger des configurations et programmes

### Commandes de Build et Test

```bash
# Compilation du projet avec les nouvelles fonctionnalités
make clean && make

# Exécution avec initrd (crée automatiquement un initrd de test)
make run

# Exécution avec interface graphique
make run-gui
```

### Résultat Attendu

Après l'implémentation complète, le système affichera :

1. Message de bienvenue habituel
2. "Gestionnaire de memoire initialise."
3. "Initrd trouve ! Fichiers:" suivi de la liste des fichiers
4. Système prêt pour les interactions clavier

### Prochaines Étapes

Ces améliorations préparent le terrain pour :

1. **Gestionnaire de tâches (Scheduler)** : Exécution de programmes multiples
2. **Moteur d'inférence IA** : Chargement et exécution d'un modèle d'IA
3. **Interface utilisateur avancée** : Shell avec commandes
4. **Optimisations** : Amélioration des performances et de la stabilité

### Notes de Développement

- Le code utilise des structures alignées pour les tables de pages
- La gestion d'erreurs est basique mais fonctionnelle
- L'implémentation privilégie la simplicité et la compréhension
- Tous les composants sont modulaires pour faciliter les extensions futures

