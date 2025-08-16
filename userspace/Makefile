# Makefile pour les programmes utilisateur AI-OS v5.0

CC = gcc
LD = ld
OBJCOPY = objcopy

# Options de compilation pour l'espace utilisateur
CFLAGS = -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector -I.
LDFLAGS = -m elf_i386 -nostdlib

# Adresse de base pour les programmes utilisateur (espace virtuel élevé)
USER_BASE = 0x40000000

# Programmes principaux de la nouvelle version
PROGRAMS = shell fake_ai test_program

# Cible par défaut : compiler tous les programmes
all: $(PROGRAMS)

# Shell interactif - Programme principal
shell: shell.c
	@echo "Compilation du shell interactif..."
	$(CC) $(CFLAGS) -c $< -o shell.o
	$(LD) $(LDFLAGS) -Ttext=$(USER_BASE) -o $@ shell.o
	@echo "Shell compilé: $@"

# Simulateur d'IA
fake_ai: fake_ai.c
	@echo "Compilation du simulateur d'IA..."
	$(CC) $(CFLAGS) -c $< -o fake_ai.o
	$(LD) $(LDFLAGS) -Ttext=$(USER_BASE) -o $@ fake_ai.o
	@echo "Simulateur IA compilé: $@"

# Programme de test original (pour compatibilité)
test_program: test_program.c
	@echo "Compilation du programme de test..."
	$(CC) $(CFLAGS) -c $< -o test_program.o
	$(LD) $(LDFLAGS) -Ttext=$(USER_BASE) -o $@ test_program.o
	@echo "Programme de test compilé: $@"

# Cibles pour créer des fichiers binaires bruts (optionnel)
shell.bin: shell
	$(OBJCOPY) -O binary $< $@
	@echo "Binaire shell créé: $@"

fake_ai.bin: fake_ai
	$(OBJCOPY) -O binary $< $@
	@echo "Binaire fake_ai créé: $@"

test_program.bin: test_program
	$(OBJCOPY) -O binary $< $@
	@echo "Binaire test_program créé: $@"

# Cible pour afficher les informations sur les exécutables
info: $(PROGRAMS)
	@echo "=== Informations sur les exécutables ==="
	@for prog in $(PROGRAMS); do \
		echo "--- $$prog ---"; \
		file $$prog; \
		ls -la $$prog; \
		echo; \
	done

# Cible pour afficher les détails ELF
info-elf: $(PROGRAMS)
	@echo "=== Détails ELF des programmes ==="
	@for prog in $(PROGRAMS); do \
		echo "--- $$prog ---"; \
		readelf -h $$prog; \
		readelf -l $$prog; \
		echo; \
	done

# Cible pour tester la compilation
test-build: all
	@echo "=== Test de compilation réussi ==="
	@echo "Programmes compilés:"
	@ls -la $(PROGRAMS)

# Cible pour nettoyer
clean:
	@echo "Nettoyage des fichiers objets et exécutables..."
	rm -f *.o $(PROGRAMS) *.bin

# Cible pour nettoyer complètement
distclean: clean
	@echo "Nettoyage complet terminé."

# Cible d'aide
help:
	@echo "Makefile pour les programmes utilisateur AI-OS"
	@echo "Cibles disponibles:"
	@echo "  all        - Compiler tous les programmes"
	@echo "  shell      - Compiler le shell interactif"
	@echo "  fake_ai    - Compiler le simulateur d'IA"
	@echo "  test_program - Compiler le programme de test"
	@echo "  info       - Afficher les informations sur les programmes"
	@echo "  info-elf   - Afficher les détails ELF"
	@echo "  test-build - Tester la compilation"
	@echo "  clean      - Nettoyer les fichiers générés"
	@echo "  distclean  - Nettoyage complet"
	@echo "  help       - Afficher cette aide"

.PHONY: all clean distclean info info-elf test-build help

