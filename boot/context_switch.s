bits 32

; Section GNU stack (pour éviter les warnings du linker)
section .note.GNU-stack noalloc noexec nowrite progbits

global context_switch

; void context_switch(cpu_state_t* old_state, cpu_state_t* new_state);
; Version simplifiée et stable du changement de contexte
context_switch:
    ; Désactive les interruptions pendant le changement de contexte
    cli
    
    ; Sauvegarde les registres de la fonction appelante
    push ebp
    mov ebp, esp
    
    ; Récupère les paramètres
    mov eax, [ebp + 8]  ; old_state
    mov ebx, [ebp + 12] ; new_state
    
    ; Vérifie si old_state est NULL (première tâche)
    test eax, eax
    jz load_new_task
    
    ; Sauvegarde l'état de l'ancienne tâche
    mov [eax + 0], eax   ; eax (sera corrigé après)
    mov [eax + 4], ebx   ; ebx (sera corrigé après)
    mov [eax + 8], ecx   ; ecx
    mov [eax + 12], edx  ; edx
    mov [eax + 16], esi  ; esi
    mov [eax + 20], edi  ; edi
    
    ; Sauvegarde EBP original (avant l'appel de fonction)
    mov ecx, [ebp]       ; EBP de l'appelant
    mov [eax + 24], ecx  ; ebp
    
    ; Sauvegarde l'adresse de retour comme EIP
    mov ecx, [ebp + 4]   ; Adresse de retour
    mov [eax + 28], ecx  ; eip
    
    ; Sauvegarde ESP (pointeur de pile avant l'appel)
    lea ecx, [ebp + 8]   ; ESP avant l'appel de fonction
    mov [eax + 32], ecx  ; esp
    
    ; Sauvegarde EFLAGS
    pushfd
    pop ecx
    mov [eax + 36], ecx  ; eflags
    
    ; Sauvegarde les registres de segment (version simplifiée)
    mov word [eax + 40], 0x08  ; cs (segment de code kernel)
    mov word [eax + 44], 0x10  ; ds (segment de données kernel)
    mov word [eax + 48], 0x10  ; es
    mov word [eax + 52], 0x10  ; fs
    mov word [eax + 56], 0x10  ; gs
    mov word [eax + 60], 0x10  ; ss
    
    ; Corrige les valeurs EAX et EBX sauvegardées
    mov ecx, [ebp + 8]   ; old_state (valeur originale d'EAX)
    mov [eax + 0], ecx
    mov ecx, [ebp + 12]  ; new_state (valeur originale d'EBX)
    mov [eax + 4], ecx

load_new_task:
    ; Vérifie si new_state est NULL
    test ebx, ebx
    jz context_switch_done
    
    ; Charge l'état de la nouvelle tâche
    ; Pour la stabilité, on garde les segments kernel pour l'instant
    ; (les tâches utilisateur seront gérées différemment)
    
    ; Charge les registres généraux
    mov eax, [ebx + 0]   ; eax
    mov ecx, [ebx + 8]   ; ecx
    mov edx, [ebx + 12]  ; edx
    mov esi, [ebx + 16]  ; esi
    mov edi, [ebx + 20]  ; edi
    
    ; Charge ESP
    mov esp, [ebx + 32]  ; esp
    
    ; Charge EBP
    mov ebp, [ebx + 24]  ; ebp
    
    ; Charge EFLAGS (mais garde les interruptions désactivées pour l'instant)
    mov ecx, [ebx + 36]  ; eflags
    and ecx, 0xFFFFFDFF  ; Désactive le flag IF (interruptions)
    push ecx
    popfd
    
    ; Prépare le saut vers la nouvelle tâche
    push dword [ebx + 28] ; EIP sur la pile
    
    ; Charge EBX en dernier
    mov ebx, [ebx + 4]   ; ebx
    
    ; Réactive les interruptions juste avant le saut
    sti
    
    ; Effectue le saut vers la nouvelle tâche
    ret  ; Utilise l'EIP sur la pile

context_switch_done:
    ; Restaure la pile et retourne
    mov esp, ebp
    pop ebp
    sti  ; Réactive les interruptions
    ret

