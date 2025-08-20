; switch_task function - Enhanced for Ring 0->3 transitions
.global switch_task
switch_task:
    push ebp
    mov ebp, esp
    
    ; Save current task context
    mov eax, [ebp + 8]  ; current_task
    test eax, eax
    jz .load_next
    
    ; Save registers
    mov [eax + 20], esp
    mov [eax + 24], ebp
    mov [eax + 28], ebx
    mov [eax + 32], esi
    mov [eax + 36], edi
    
.load_next:
    mov edx, [ebp + 12] ; next_task
    test edx, edx
    jz .done
    
    ; Check privilege level
    mov bl, [edx + 52]  ; task->privilege_level
    cmp bl, 3
    je .switch_to_user
    
    ; Kernel task - standard switch
    jmp .kernel_switch
    
.switch_to_user:
    ; Switch page directory
    mov ebx, [edx + 16]  ; task->page_directory
    push edx
    push ebx
    call switch_page_directory
    add esp, 4
    pop edx
    
    ; Set up user mode transition
    mov esp, [edx + 20]  ; User stack
    mov ebx, [edx + 56]  ; Entry point
    
    ; Push user mode context for iret
    push 0x23           ; USER_DATA_SELECTOR (SS)
    push esp            ; ESP
    pushfd              ; EFLAGS
    pop ecx
    or ecx, 0x200       ; Enable interrupts
    push ecx            ; EFLAGS
    push 0x1B           ; USER_CODE_SELECTOR (CS)
    push ebx            ; EIP
    
    ; Load user segments
    mov ax, 0x23        ; USER_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Jump to Ring 3
    iret
    
.kernel_switch:
    ; Standard kernel task switching
    mov ebx, [edx + 16]     ; page_directory
    mov ecx, cr3
    cmp ebx, ecx
    je .same_dir
    
    push edx
    push ebx
    call switch_page_directory
    add esp, 4
    pop edx
    
.same_dir:
    mov esp, [edx + 20]
    mov ebp, [edx + 24]
    mov ebx, [edx + 28]
    mov esi, [edx + 32]
    mov edi, [edx + 36]
    
.done:
    sti
    ret
