# Outils de compilation
CC = gcc
AS = nasm
LD = ld

# Options de compilation
# -m32 : Compiler en 32-bit
# -ffreestanding : Ne pas utiliser la bibliothèque standard C
# -nostdlib : Ne pas lier avec la bibliothèque standard C
# -fno-pie : Produire du code indépendant de la position
CFLAGS = -m32 -ffreestanding -nostdlib -fno-pie -Wall -Wextra -I. -Iinclude
ASFLAGS = -f elf32

# Nom du fichier final de notre OS
OS_IMAGE = build/ai_os.bin
ISO_IMAGE = build/ai_os.iso
INITRD_IMAGE = my_initrd.tar

# Variables pour la création de l'initrd
USER_SHELL := userspace/shell
INITRD_DIR := initrd_content
BIN_DEST_DIR := $(INITRD_DIR)/bin

# Liste des fichiers objets - MISE À JOUR avec tous les nouveaux fichiers
OBJECTS = build/boot.o build/idt_loader.o build/isr_stubs.o build/paging.o build/context_switch.o build/userspace_switch.o \
          build/string.o build/pmm.o build/heap.o build/gdt_asm.o build/gdt.o build/idt.o build/vmm.o build/task.o \
          build/syscall.o build/elf.o build/initrd.o build/interrupts.o \
          build/keyboard.o build/timer.o build/multiboot.o build/kernel.o build/kbd_buffer.o

# Cible par défaut : construire le système complet (noyau + initrd)
all: $(OS_IMAGE) pack-initrd
	@echo "=== AI-OS v5.0 - Système Complet Construit ==="
	@echo "Noyau: $(OS_IMAGE) ($(shell ls -lh $(OS_IMAGE) | awk '{print $$5}'))"
	@echo "Initrd: $(INITRD_IMAGE) ($(shell ls -lh $(INITRD_IMAGE) | awk '{print $$5}'))"
	@echo "Système prêt pour exécution avec: make run"

# Règle pour lier les fichiers objets et créer l'image finale
$(OS_IMAGE): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(LD) -m elf_i386 -T linker.ld -o $@ --start-group $(OBJECTS) --end-group

# Cible pour compiler seulement le noyau (sans initrd)
kernel-only: $(OS_IMAGE)
	@echo "=== Noyau AI-OS Compilé ==="
	@echo "Fichier: $(OS_IMAGE) ($(shell ls -lh $(OS_IMAGE) | awk '{print $$5}'))"

# Règles de compilation pour les fichiers .c du kernel principal
build/kernel.o: kernel/kernel.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/gdt.o: kernel/gdt.c kernel/gdt.h
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

build/kbd_buffer.o: kernel/input/kbd_buffer.c kernel/input/kbd_buffer.h
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

build/heap.o: kernel/mem/heap.c kernel/mem/heap.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/string.o: kernel/mem/string.c kernel/mem/string.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Règles de compilation pour le système de tâches (version complète)
build/task.o: kernel/task/task.c kernel/task/task.h
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

build/gdt_asm.o: boot/gdt.s
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

build/context_switch.o: boot/context_switch_new.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

build/userspace_switch.o: boot/userspace_switch.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Règle pour empaqueter l'initrd automatiquement
pack-initrd: $(USER_SHELL) userspace/fake_ai userspace/test_program
	@echo "[mkinitrd] Création de l'initrd AI-OS v5.0..."
	@mkdir -p $(BIN_DEST_DIR)
	@echo "Ceci est un fichier de test depuis l'initrd !" > $(INITRD_DIR)/test.txt
	@echo "Un autre fichier de demonstration." > $(INITRD_DIR)/hello.txt
	@echo "Configuration du systeme AI-OS v5.0" > $(INITRD_DIR)/config.cfg
	@echo "#!/bin/sh" > $(INITRD_DIR)/startup.sh
	@echo "echo 'Script de demarrage AI-OS v5.0'" >> $(INITRD_DIR)/startup.sh
	@echo "Donnees pour l'intelligence artificielle simulee" > $(INITRD_DIR)/ai_data.txt
	@echo "Base de connaissances IA - Version simulation" > $(INITRD_DIR)/ai_knowledge.txt
	@cp -f $(USER_SHELL) $(BIN_DEST_DIR)/shell
	@cp -f userspace/fake_ai $(BIN_DEST_DIR)/fake_ai
	@cp -f userspace/test_program $(BIN_DEST_DIR)/user_program
	@tar -C $(INITRD_DIR) -cf $(INITRD_IMAGE) .
	@echo "[mkinitrd] Packed executables into $(INITRD_IMAGE)"

# Compile tous les programmes utilisateur
userspace/shell userspace/fake_ai userspace/test_program:
	@echo "Compilation des programmes utilisateur AI-OS v5.0..."
	@$(MAKE) -C userspace all

# Cible pour exécuter l'OS dans QEMU avec initrd
run: $(OS_IMAGE) pack-initrd
	qemu-system-i386 -kernel $(OS_IMAGE) -initrd $(INITRD_IMAGE) -nographic -serial stdio -d int,guest_errors,cpu_reset -no-reboot -no-shutdown -monitor none

# Cible pour exécuter l'OS dans QEMU avec interface graphique améliorée
run-gui: $(OS_IMAGE) pack-initrd
	qemu-system-i386 -kernel $(OS_IMAGE) -initrd $(INITRD_IMAGE) -m 256M -vga std -no-reboot -d int -rtc base=utc

# Cible pour tester la compilation sans exécution
test-build: $(OS_IMAGE)
	@echo "Compilation réussie ! Image générée: $(OS_IMAGE)"
	@ls -la $(OS_IMAGE)

# Cible pour afficher les informations sur l'initrd
info-initrd: pack-initrd
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
	@echo "=== AI-OS v5.0 - Cibles de Compilation Disponibles ==="
	@echo ""
	@echo "Cibles principales:"
	@echo "  all          - Compile le système complet (noyau + initrd + programmes)"
	@echo "  kernel-only  - Compile seulement le noyau"
	@echo "  run          - Compile et exécute avec QEMU (mode texte)"
	@echo "  run-gui      - Compile et exécute avec QEMU (mode graphique)"
	@echo ""
	@echo "Cibles de développement:"
	@echo "  test-build   - Compile sans exécuter"
	@echo "  user-program - Compile seulement les programmes utilisateur"
	@echo "  info-initrd  - Affiche le contenu de l'initrd"
	@echo "  info-user    - Affiche les infos des programmes utilisateur"
	@echo ""
	@echo "Cibles de maintenance:"
	@echo "  clean        - Nettoie les fichiers générés"
	@echo "  distclean    - Nettoie tout (y compris sauvegardes)"
	@echo "  help         - Affiche cette aide"
	@echo ""
	@echo "Usage recommandé:"
	@echo "  make clean && make all    # Compilation complète"
	@echo "  make run                  # Test rapide"

.PHONY: all kernel-only run run-gui test-build info-initrd info-user user-program clean distclean help pack-initrd

