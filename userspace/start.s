.section .note.GNU-stack,"",@progbits

.section .text
.global _start

_start:
    # Initialiser la pile
    movl $0x50000000, %esp  # Pile utilisateur à 0x50000000
    
    # Appeler main()
    call main
    
    # Si main() retourne, appeler exit
    movl %eax, %ebx         # Code de retour de main
    movl $0, %eax           # SYS_EXIT
    int $0x80               # Appel système
    
    # Boucle infinie au cas où
    jmp .

