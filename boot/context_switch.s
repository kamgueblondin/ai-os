bits 32

global context_switch

; void context_switch(cpu_state_t* old_state, cpu_state_t* new_state);
; Cette fonction effectue un changement de contexte entre deux tâches
context_switch:
    ; Sauvegarde les registres de la fonction appelante
    push ebp
    mov ebp, esp
    
    ; Récupère les paramètres
    mov eax, [ebp + 8]  ; old_state
    mov ebx, [ebp + 12] ; new_state
    
    ; Sauvegarde l'état de l'ancienne tâche
    ; Sauvegarde les registres généraux
    mov [eax + 0], eax   ; eax (sera écrasé, mais on le sauve quand même)
    mov [eax + 4], ebx   ; ebx
    mov [eax + 8], ecx   ; ecx
    mov [eax + 12], edx  ; edx
    mov [eax + 16], esi  ; esi
    mov [eax + 20], edi  ; edi
    mov [eax + 24], ebp  ; ebp
    
    ; Sauvegarde EIP (adresse de retour)
    mov ecx, [ebp + 4]   ; Adresse de retour de context_switch
    mov [eax + 28], ecx  ; eip
    
    ; Sauvegarde ESP (pointeur de pile)
    mov [eax + 32], esp  ; esp
    
    ; Sauvegarde EFLAGS
    pushfd
    pop ecx
    mov [eax + 36], ecx  ; eflags
    
    ; Sauvegarde les registres de segment
    mov cx, cs
    mov [eax + 40], cx   ; cs
    mov cx, ds
    mov [eax + 44], cx   ; ds
    mov cx, es
    mov [eax + 48], cx   ; es
    mov cx, fs
    mov [eax + 52], cx   ; fs
    mov cx, gs
    mov [eax + 56], cx   ; gs
    mov cx, ss
    mov [eax + 60], cx   ; ss
    
    ; Charge l'état de la nouvelle tâche
    ; Charge les registres de segment
    mov cx, [ebx + 44]   ; ds
    mov ds, cx
    mov cx, [ebx + 48]   ; es
    mov es, cx
    mov cx, [ebx + 52]   ; fs
    mov fs, cx
    mov cx, [ebx + 56]   ; gs
    mov gs, cx
    mov cx, [ebx + 60]   ; ss
    mov ss, cx
    
    ; Charge EFLAGS
    mov ecx, [ebx + 36]  ; eflags
    push ecx
    popfd
    
    ; Charge ESP
    mov esp, [ebx + 32]  ; esp
    
    ; Charge les registres généraux
    mov eax, [ebx + 0]   ; eax
    mov ecx, [ebx + 8]   ; ecx
    mov edx, [ebx + 12]  ; edx
    mov esi, [ebx + 16]  ; esi
    mov edi, [ebx + 20]  ; edi
    mov ebp, [ebx + 24]  ; ebp
    
    ; Charge EIP et effectue le saut
    push dword [ebx + 40] ; cs
    push dword [ebx + 28] ; eip
    
    ; Charge EBX en dernier
    mov ebx, [ebx + 4]   ; ebx
    
    ; Effectue le saut vers la nouvelle tâche
    retf  ; Retour far (utilise CS:EIP sur la pile)

