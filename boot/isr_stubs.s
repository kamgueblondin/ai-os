bits 32

extern keyboard_handler
extern timer_handler
extern syscall_handler

global irq0
global irq1
global isr_syscall

; ISR pour le timer (IRQ 0)
irq0:
    pusha                 ; Sauvegarde tous les registres
    call timer_handler    ; Appelle notre fonction C du timer
    popa                  ; Restaure les registres
    iret                  ; Retour d'interruption

; ISR pour le clavier (IRQ 1)
irq1:
    pusha                 ; Sauvegarde tous les registres
    call keyboard_handler ; Appelle notre fonction C
    popa                  ; Restaure les registres
    iret                  ; Retour d'interruption

; ISR pour les appels système (INT 0x80)
isr_syscall:
    ; Sauvegarde l'état complet du CPU
    push ds
    push es
    push fs
    push gs
    pusha
    
    ; Charge les segments du noyau
    mov ax, 0x10  ; Segment de données du noyau
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Prépare la structure cpu_state_t sur la pile
    ; L'ordre doit correspondre à la structure dans task.h
    push esp      ; Pointeur vers la structure
    
    ; Appelle le handler C des syscalls
    call syscall_handler
    
    ; Nettoie la pile
    add esp, 4
    
    ; Restaure l'état du CPU
    popa
    pop gs
    pop fs
    pop es
    pop ds
    
    ; Retour d'interruption
    iret

