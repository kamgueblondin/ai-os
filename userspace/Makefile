# Makefile pour les programmes utilisateur AI-OS

CC = gcc
LD = ld
OBJCOPY = objcopy

# Options de compilation pour l'espace utilisateur
CFLAGS = -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector -I.
LDFLAGS = -m elf_i386 -nostdlib

# Adresse de base pour les programmes utilisateur (espace virtuel élevé)
USER_BASE = 0x40000000

# Programme de test principal
test_program: test_program.c
	@echo "Compilation du programme utilisateur..."
	$(CC) $(CFLAGS) -c $< -o test_program.o
	$(LD) $(LDFLAGS) -Ttext=$(USER_BASE) -o $@ test_program.o
	@echo "Programme utilisateur compilé: $@"

# Cible pour créer un fichier binaire brut (optionnel)
test_program.bin: test_program
	$(OBJCOPY) -O binary $< $@
	@echo "Binaire brut créé: $@"

# Cible pour afficher les informations sur l'exécutable
info: test_program
	@echo "=== Informations sur l'exécutable ==="
	file test_program
	readelf -h test_program
	readelf -l test_program
	@echo "Taille du fichier:"
	ls -la test_program

# Cible pour nettoyer
clean:
	rm -f *.o test_program test_program.bin

# Cible pour tout construire
all: test_program test_program.bin

.PHONY: clean all info

