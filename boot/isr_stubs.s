bits 32

extern keyboard_handler

global irq1

; ISR pour le clavier (IRQ 1)
irq1:
    pusha                 ; Sauvegarde tous les registres
    call keyboard_handler ; Appelle notre fonction C
    popa                  ; Restaure les registres
    iret                  ; Retour d'interruption

