# Options de compilation pour l'espace utilisateur
CC = gcc
LD = ld
AS = as

CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -I../kernel -I../include
LDFLAGS = -m elf_i386 -nostdlib -Ttext=0x40000000
ASFLAGS = --32

# Programmes à compiler
PROGRAMS = shell fake_ai test_program ai_assistant stress_test

all: $(PROGRAMS)

shell: shell.o start.o
	@echo "Linking shell..."
	$(LD) -m elf_i386 $(LDFLAGS) -o $@ $^

fake_ai: fake_ai.o start.o
	@echo "Linking fake_ai..."
	$(LD) -m elf_i386 $(LDFLAGS) -o $@ $^

ai_assistant: ai_assistant.o start.o
	@echo "Linking ai_assistant..."
	$(LD) -m elf_i386 $(LDFLAGS) -o $@ $^

test_program: test_program.o start.o
	@echo "Linking test_program..."
	$(LD) -m elf_i386 $(LDFLAGS) -o $@ $^

stress_test: stress_test.o start.o
	@echo "Linking stress_test..."
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
