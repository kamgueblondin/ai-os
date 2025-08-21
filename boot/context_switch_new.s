[BITS 32]

section .text

global jump_to_task, switch_task

; void jump_to_task(cpu_state_t* next_state)
; Ne sauvegarde rien, charge juste le nouvel état.
jump_to_task:
    mov ebx, [esp + 4]  ; Pointeur vers next_state

    ; Charger les registres de segment de données utilisateur
    mov ax, [ebx + 44]  ; ds
    mov ds, ax
    mov ax, [ebx + 48]  ; es
    mov es, ax
    mov ax, [ebx + 52]  ; fs
    mov fs, ax
    mov ax, [ebx + 56]  ; gs
    mov gs, ax

    ; Préparer la pile pour iret
    push dword [ebx + 60] ; ss
    push dword [ebx + 32] ; esp
    push dword [ebx + 36] ; eflags
    push dword [ebx + 40] ; cs
    push dword [ebx + 28] ; eip

    ; Charger les registres généraux
    mov ebp, [ebx + 24]
    mov edi, [ebx + 20]
    mov esi, [ebx + 16]
    mov edx, [ebx + 12]
    mov ecx, [ebx + 8]
    ; eax et ebx sont chargés à la fin

    ; Charger ebx
    mov ebx, [ebx + 4]

    ; Charger eax (le pointeur original est perdu, on le recharge)
    mov eax, [esp + 4]
    mov eax, [eax]

    ; Saut vers le mode utilisateur
    iret

; void switch_task(cpu_state_t* old_state, cpu_state_t* new_state)
; Sauvegarde l'état actuel et charge le nouvel état.
switch_task:
    ; Récupérer les arguments de la pile
    mov eax, [esp + 4]  ; old_state
    mov ebx, [esp + 8]  ; new_state

    ; Sauvegarder les registres généraux
    mov [eax + 0], eax
    mov [eax + 4], ebx
    mov [eax + 8], ecx
    mov [eax + 12], edx
    mov [eax + 16], esi
    mov [eax + 20], edi
    mov [eax + 24], ebp

    ; Sauvegarder eip (pointeur d'instruction)
    ; On prend l'adresse de retour de la pile
    mov ecx, [esp]
    mov [eax + 28], ecx

    ; Sauvegarder esp (pointeur de pile)
    ; L'ESP actuel est celui avant l'appel à cette fonction
    mov [eax + 32], esp

    ; Sauvegarder eflags
    pushfd
    pop dword [eax + 36]

    ; Sauvegarder les registres de segment
    mov [eax + 40], cs
    mov [eax + 44], ds
    mov [eax + 48], es
    mov [eax + 52], fs
    mov [eax + 56], gs
    mov [eax + 60], ss

    ; --- Charger le nouvel état ---

    ; Charger les registres de segment de données
    mov ax, [ebx + 44]
    mov ds, ax
    mov ax, [ebx + 48]
    mov es, ax
    mov ax, [ebx + 52]
    mov fs, ax
    mov ax, [ebx + 56]
    mov gs, ax

    ; Changer de pile
    mov esp, [ebx + 32]

    ; Charger les registres généraux
    mov ebp, [ebx + 24]
    mov edi, [ebx + 20]
    mov esi, [ebx + 16]
    mov edx, [ebx + 12]
    mov ecx, [ebx + 8]
    mov eax, [ebx + 0]
    mov ebx, [ebx + 4]

    ; Sauter à la nouvelle tâche
    jmp [ebx + 28]
