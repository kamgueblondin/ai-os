# Outils de compilation
CC = gcc
AS = nasm
LD = ld

# Options de compilation
# -m32 : Compiler en 32-bit
# -ffreestanding : Ne pas utiliser la bibliothèque standard C
# -nostdlib : Ne pas lier avec la bibliothèque standard C
# -fno-pie : Produire du code indépendant de la position
CFLAGS = -m32 -ffreestanding -nostdlib -fno-pie -Wall -Wextra -I.
ASFLAGS = -f elf32

# Nom du fichier final de notre OS
OS_IMAGE = build/ai_os.bin
ISO_IMAGE = build/ai_os.iso
INITRD_IMAGE = my_initrd.tar

# Liste des fichiers objets - MISE À JOUR avec tous les nouveaux fichiers
OBJECTS = build/boot.o build/idt_loader.o build/isr_stubs.o build/paging.o build/context_switch.o \
          build/kernel.o build/idt.o build/interrupts.o build/keyboard.o build/timer.o \
          build/multiboot.o build/pmm.o build/vmm.o build/initrd.o \
          build/task.o build/syscall.o build/elf.o

# Cible par défaut : construire l'image de l'OS
all: $(OS_IMAGE)

# Règle pour lier les fichiers objets et créer l'image finale
$(OS_IMAGE): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(LD) -m elf_i386 -T linker.ld -o $@ $(OBJECTS)

# Règles de compilation pour les fichiers .c du kernel principal
build/kernel.o: kernel/kernel.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/idt.o: kernel/idt.c kernel/idt.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	
build/interrupts.o: kernel/interrupts.c kernel/interrupts.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/keyboard.o: kernel/keyboard.c kernel/keyboard.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/timer.o: kernel/timer.c kernel/timer.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/multiboot.o: kernel/multiboot.c kernel/multiboot.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/elf.o: kernel/elf.c kernel/elf.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Règles de compilation pour les fichiers de gestion mémoire
build/pmm.o: kernel/mem/pmm.c kernel/mem/pmm.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/vmm.o: kernel/mem/vmm.c kernel/mem/vmm.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Règles de compilation pour le système de tâches (version stable)
build/task.o: kernel/task/task_stable.c kernel/task/task.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Règles de compilation pour les appels système
build/syscall.o: kernel/syscall/syscall.c kernel/syscall/syscall.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Règles de compilation pour le système de fichiers
build/initrd.o: fs/initrd.c fs/initrd.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Règles pour compiler le code assembleur
build/boot.o: boot/boot.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

build/idt_loader.o: boot/idt_loader.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

build/isr_stubs.o: boot/isr_stubs.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

build/paging.o: boot/paging.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

build/context_switch.o: boot/context_switch.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Règle pour créer l'initrd de test avec le programme utilisateur
$(INITRD_IMAGE): userspace/test_program
	@echo "Création de l'initrd avec programme utilisateur..."
	@mkdir -p initrd_content
	@echo "Ceci est un fichier de test depuis l'initrd !" > initrd_content/test.txt
	@echo "Un autre fichier de demonstration." > initrd_content/hello.txt
	@echo "Configuration du systeme AI-OS" > initrd_content/config.cfg
	@echo "#!/bin/sh" > initrd_content/startup.sh
	@echo "echo 'Script de demarrage AI-OS'" >> initrd_content/startup.sh
	@echo "Donnees pour l'intelligence artificielle" > initrd_content/ai_data.txt
	@cp userspace/test_program initrd_content/user_program
	@tar -cf $(INITRD_IMAGE) -C initrd_content .
	@echo "Initrd créé avec programme utilisateur: $(INITRD_IMAGE)"

# Compile le programme utilisateur
userspace/test_program:
	@echo "Compilation du programme utilisateur..."
	@$(MAKE) -C userspace test_program

# Cible pour exécuter l'OS dans QEMU avec initrd
run: $(OS_IMAGE) $(INITRD_IMAGE)
	qemu-system-i386 -kernel $(OS_IMAGE) -initrd $(INITRD_IMAGE) -nographic -serial file:output.log

# Cible pour exécuter l'OS dans QEMU avec interface graphique
run-gui: $(OS_IMAGE) $(INITRD_IMAGE)
	qemu-system-i386 -kernel $(OS_IMAGE) -initrd $(INITRD_IMAGE)

# Cible pour tester la compilation sans exécution
test-build: $(OS_IMAGE)
	@echo "Compilation réussie ! Image générée: $(OS_IMAGE)"
	@ls -la $(OS_IMAGE)

# Cible pour afficher les informations sur l'initrd
info-initrd: $(INITRD_IMAGE)
	@echo "Contenu de l'initrd:"
	@tar -tvf $(INITRD_IMAGE)

# Cible pour compiler seulement le programme utilisateur
user-program:
	@$(MAKE) -C userspace all

# Cible pour afficher les informations sur le programme utilisateur
info-user:
	@$(MAKE) -C userspace info

# Cible pour nettoyer le projet
clean:
	rm -rf build
	rm -f $(INITRD_IMAGE)
	rm -rf initrd_content
	rm -f output.log
	@$(MAKE) -C userspace clean

# Cible pour nettoyer complètement (y compris les fichiers de sauvegarde)
distclean: clean
	rm -f *~ */*~ */*/*~
	rm -f *.bak */*.bak */*/*.bak

# Cible pour afficher l'aide
help:
	@echo "Cibles disponibles:"
	@echo "  all        - Compile le système (défaut)"
	@echo "  run        - Compile et exécute avec QEMU (mode texte)"
	@echo "  run-gui    - Compile et exécute avec QEMU (mode graphique)"
	@echo "  test-build - Compile sans exécuter"
	@echo "  user-program - Compile seulement le programme utilisateur"
	@echo "  info-initrd- Affiche le contenu de l'initrd"
	@echo "  info-user  - Affiche les infos du programme utilisateur"
	@echo "  clean      - Nettoie les fichiers générés"
	@echo "  distclean  - Nettoie tout (y compris sauvegardes)"
	@echo "  help       - Affiche cette aide"

.PHONY: all run run-gui test-build info-initrd info-user user-program clean distclean help

