bits 32

; Section GNU stack (pour éviter les warnings du linker)
section .note.GNU-stack noalloc noexec nowrite progbits

global idt_load

idt_load:
    ; Récupère le pointeur vers la structure idt_ptr passé en argument sur la pile
    mov eax, [esp + 4]
    lidt [eax]  ; Charge le registre IDTR avec notre pointeur
    ret

