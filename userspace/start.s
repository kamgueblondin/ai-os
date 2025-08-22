.section .note.GNU-stack,"",@progbits

.section .text
.global _start

_start:
    # La pile est déjà initialisée par le noyau.
    # On peut l'utiliser directement.
    
    # Appeler main()
    call main
    
    # Si main() retourne, appeler exit
    movl %eax, %ebx         # Code de retour de main
    movl $0, %eax           # SYS_EXIT
    int $0x80               # Appel système
    
    # Boucle infinie au cas où
    jmp .

