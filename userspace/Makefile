# Options de compilation pour l'espace utilisateur
CC = gcc
LD = ld
AS = as

CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -I../kernel -I../include
LDFLAGS = -nostdlib -Ttext=0x40000000
ASFLAGS = --32

# Programmes à compiler
PROGRAMS = shell fake_ai test_program

all: $(PROGRAMS)

shell: shell.o start.o
	@echo "Linking shell..."
	$(LD) -m elf_i386 $(LDFLAGS) -o $@ $^

fake_ai: fake_ai.o start.o
	@echo "Linking fake_ai..."
	$(LD) -m elf_i386 $(LDFLAGS) -o $@ $^

test_program: test_program.o start.o
	@echo "Linking test_program..."
	$(LD) -m elf_i386 $(LDFLAGS) -o $@ $^

%.o: %.c
	@echo "Compiling C: $<"
	$(CC) $(CFLAGS) -c -o $@ $<

start.o: start.s
	@echo "Assembling with AS: $<"
	$(AS) $(ASFLAGS) $< -o $@

clean:
	@echo "Cleaning userspace..."
	rm -f *.o $(PROGRAMS)
