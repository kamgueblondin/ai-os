bits 32

section .note.GNU-stack noalloc noexec nowrite progbits

section .multiboot
align 4
    MULTIBOOT_HEADER_MAGIC: equ 0x1BADB002
    MULTIBOOT_HEADER_FLAGS: equ 0x0001 ; Aligner les modules
    CHECKSUM: equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd CHECKSUM

section .text

gdt_start:
    ; Descripteur nul
    dd 0x0, 0x0
    ; Descripteur de code noyau
gdt_code: dw 0xFFFF, 0x0000, 0x9A00, 0x00CF
    ; Descripteur de données noyau
gdt_data: dw 0xFFFF, 0x0000, 0x9200, 0x00CF
    ; Descripteur de code utilisateur
gdt_user_code: dw 0xFFFF, 0x0000, 0xFA00, 0x00CF
    ; Descripteur de données utilisateur
gdt_user_data: dw 0xFFFF, 0x0000, 0xF200, 0x00CF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

global _start
_start:
    ; 1. Mettre en place la pile
    mov esp, stack_top

    ; 2. Charger la GDT
    lgdt [gdt_descriptor]
    jmp CODE_SEG:.flush_cs

.flush_cs:
    ; 3. Recharger les segments de données en utilisant CX pour ne pas corrompre EAX
    mov cx, DATA_SEG
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    mov ss, cx

    ; 4. Passer les infos Multiboot au noyau (EAX et EBX sont maintenant préservés)
    push ebx
    push eax

    extern kmain
    call kmain

    cli
    hlt

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
