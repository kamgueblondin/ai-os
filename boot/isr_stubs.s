extern keyboard_handler
extern timer_handler
extern syscall_handler

global irq0
global irq1
global isr_syscall

; ISR pour le timer (IRQ 0) - Version robuste
irq0:
    ; Sauvegarde complète de l'état du processeur
    push ds
    push es
    push fs
    push gs
    pushad
    
    ; Charge les segments du noyau
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Passe un pointeur vers la structure de registres au handler C
    push esp
    call timer_handler
    add esp, 4
    
    ; Envoie EOI au PIC pour IRQ 0
    mov al, 0x20
    out 0x20, al
    
    ; Restaure l'état complet du processeur
    popad
    pop gs
    pop fs
    pop es
    pop ds
    
    ; Retour d'interruption
    iret

; ISR pour le clavier (IRQ 1) - Version robuste
irq1:
    ; Sauvegarde complète de l'état du processeur
    push ds               ; Sauvegarde des segments
    push es
    push fs
    push gs
    pushad                ; Sauvegarde EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    
    ; Charge les segments du noyau
    mov ax, 0x10          ; Segment de données du noyau
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Appelle le handler C du clavier
    call keyboard_handler
    
    ; Envoie EOI au PIC pour IRQ 1
    mov al, 0x20          ; Commande EOI
    out 0x20, al          ; Envoie à PIC1
    
    ; Restaure l'état complet du processeur
    popad                 ; Restaure tous les registres généraux
    pop gs
    pop fs
    pop es
    pop ds
    
    ; Retour d'interruption
    iret

; ISR pour le scheduler volontaire (INT 0x30)
global isr_schedule
isr_schedule:
    push ds
    push es
    push fs
    push gs
    pushad

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call timer_handler ; Le timer handler appelle schedule, c'est ce qu'on veut
    add esp, 4

    popad
    pop gs
    pop fs
    pop es
    pop ds

    iret

; ISR pour les appels système (INT 0x80)
isr_syscall:
    ; Sauvegarde l'état complet du CPU
    push ds
    push es
    push fs
    push gs
    pushad
    
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
    popad
    pop gs
    pop fs
    pop es
    pop ds
    
    ; Retour d'interruption
    iret

; Section GNU stack (sécurité - pile non exécutable)
section .note.GNU-stack
