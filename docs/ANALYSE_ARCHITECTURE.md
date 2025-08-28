# Analyse de l'Architecture AI-OS

## Vue d'ensemble du système

AI-OS est un système d'exploitation expérimental en 32-bit avec les composants suivants :

### Composants principaux

1. **Kernel (kernel/)**
   - `kernel.c` : Noyau principal avec initialisation et gestion système
   - `idt.c/h` : Gestion de la table des descripteurs d'interruption
   - `interrupts.c/h` : Gestionnaire d'interruptions
   - `keyboard.c/h` : Pilote clavier
   - `timer.c/h` : Gestionnaire de timer
   - `multiboot.c/h` : Support du protocole Multiboot

2. **Gestion mémoire (kernel/mem/)**
   - `pmm.c/h` : Physical Memory Manager
   - `vmm.c/h` : Virtual Memory Manager

3. **Système de tâches (kernel/task/)**
   - `task.c/h` : Gestion des processus et ordonnancement
   - Support pour tâches kernel et utilisateur

4. **Appels système (kernel/syscall/)**
   - `syscall.c/h` : Interface kernel/userspace
   - 7 appels système définis (exit, putc, getc, puts, yield, gets, exec)

5. **Chargeur ELF (kernel/)**
   - `elf.c/h` : Chargement d'exécutables ELF

6. **Système de fichiers (fs/)**
   - `initrd.c/h` : Système de fichiers initial en mémoire (format TAR)

7. **Code assembleur (boot/)**
   - `boot.s` : Point d'entrée du système
   - `context_switch.s` : Changement de contexte entre tâches
   - `idt_loader.s`, `isr_stubs.s` : Support interruptions
   - `paging.s` : Support pagination mémoire

8. **Programmes utilisateur (userspace/)**
   - `shell.c` : Shell interactif avancé avec IA intégrée
   - `fake_ai.c` : Simulateur d'intelligence artificielle
   - `test_program.c` : Programme de test

## Problèmes identifiés

### 1. Problème principal : Changement de contexte désactivé

**Localisation :** `kernel/task/task.c`, ligne 124
```c
// Changement de contexte (désactivé temporairement)
// switch_task(&old_task->cpu_state, &current_task->cpu_state);
```

**Impact :** Le passage au mode utilisateur ne fonctionne pas car le changement de contexte est commenté.

### 2. Problèmes de configuration des tâches utilisateur

**Localisation :** `kernel/task/task.c`, fonction `create_user_task()`

**Problèmes :**
- Utilise `kernel_directory` au lieu d'un répertoire de pages dédié pour l'utilisateur
- Configuration des segments utilisateur (CS=0x1B, DS=0x23) mais pas de GDT appropriée

### 3. Problèmes de syntaxe corrigés

**Localisation :** `kernel/kernel.c`
- Accolade fermante manquante dans la boucle du shell kernel (corrigé)

### 4. Dépendances manquantes

**Problèmes résolus :**
- Installation de `nasm` pour l'assembleur
- Installation de `gcc-multilib` pour la compilation 32-bit

## Architecture du passage au mode utilisateur

### Flux prévu :
1. `kmain()` initialise le système
2. Charge le shell depuis l'initrd avec `elf_load()`
3. Crée une tâche utilisateur avec `create_user_task()`
4. Appelle `schedule()` pour passer au mode utilisateur
5. Le shell utilisateur s'exécute et interagit avec l'IA

### Flux actuel (problématique) :
1. `kmain()` initialise le système
2. Charge le shell depuis l'initrd avec `elf_load()`
3. Crée une tâche utilisateur avec `create_user_task()`
4. Appelle `schedule()` mais le changement de contexte est désactivé
5. Retombe sur le shell kernel de secours
6. Le système redémarre en boucle

## Recommandations de correction

### 1. Réactiver le changement de contexte
- Décommenter la ligne dans `schedule()`
- Tester le changement de contexte assembleur

### 2. Corriger la gestion des tâches utilisateur
- Créer un répertoire de pages dédié pour l'utilisateur
- Vérifier la configuration de la GDT pour les segments utilisateur

### 3. Améliorer la stabilité
- Ajouter des vérifications d'erreur
- Améliorer la gestion des exceptions

### 4. Tests progressifs
- Tester d'abord avec des tâches kernel simples
- Puis progresser vers les tâches utilisateur

