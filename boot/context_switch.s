bits 32

global context_switch

; void context_switch(cpu_state_t* old_state, cpu_state_t* new_state);
; Enhanced version with Ring 0->3 transition support
context_switch:
    ; Disable interrupts during context change
    cli
    
    ; Save caller's registers
    push ebp
    mov ebp, esp
    
    ; Get parameters
    mov eax, [ebp + 8]  ; old_state
    mov ebx, [ebp + 12] ; new_state
    
    ; Check if old_state is NULL (first task)
    test eax, eax
    jz load_new_task
    
    ; Save old task state
    mov [eax + 0], eax    ; eax (will be corrected after)
    mov [eax + 4], ebx    ; ebx (will be corrected after)
    mov [eax + 8], ecx    ; ecx
    mov [eax + 12], edx   ; edx
    mov [eax + 16], esi   ; esi
    mov [eax + 20], edi   ; edi
    
    ; Save original EBP (before function call)
    mov ecx, [ebp]        ; Caller's EBP
    mov [eax + 24], ecx   ; ebp
    
    ; Save return address as EIP
    mov ecx, [ebp + 4]    ; Return address
    mov [eax + 28], ecx   ; eip
    
    ; Save ESP (stack pointer before call)
    lea ecx, [ebp + 8]    ; ESP before function call
    mov [eax + 32], ecx   ; esp
    
    ; Save EFLAGS
    pushfd
    pop ecx
    mov [eax + 36], ecx   ; eflags
    
    ; Save segment registers (simplified version)
    mov word [eax + 40], 0x08   ; cs (kernel code segment)
    mov word [eax + 44], 0x10   ; ds (kernel data segment)
    mov word [eax + 48], 0x10   ; es
    mov word [eax + 52], 0x10   ; fs
    mov word [eax + 56], 0x10   ; gs
    mov word [eax + 60], 0x10   ; ss
    
    ; Correct saved EAX and EBX values
    mov ecx, [ebp + 8]     ; old_state (original EAX value)
    mov [eax + 0], ecx
    mov ecx, [ebp + 12]    ; new_state (original EBX value)
    mov [eax + 4], ecx

load_new_task:
    ; Check if new_state is NULL
    test ebx, ebx
    jz context_switch_done
    
    ; Check if this is a user mode task (privilege level 3)
    cmp byte [ebx + 64], 3  ; Check privilege_level field
    je load_user_task
    
    ; Load kernel task state
    ; For stability, keep kernel segments for now
    ; (user tasks will be handled differently)
    
    ; Load general registers
    mov eax, [ebx + 0]    ; eax
    mov ecx, [ebx + 8]    ; ecx
    mov edx, [ebx + 12]   ; edx
    mov esi, [ebx + 16]   ; esi
    mov edi, [ebx + 20]   ; edi
    
    ; Load ESP
    mov esp, [ebx + 32]   ; esp
    
    ; Load EBP
    mov ebp, [ebx + 24]   ; ebp
    
    ; Load EFLAGS (but keep interrupts disabled for now)
    mov ecx, [ebx + 36]   ; eflags
    and ecx, 0xFFFFFDFF   ; Disable IF flag (interrupts)
    push ecx
    popfd
    
    ; Prepare jump to new task
    push dword [ebx + 28] ; EIP on stack
    
    ; Load EBX last
    mov ebx, [ebx + 4]    ; ebx
    
    ; Reactivate interrupts just before jump
    sti
    
    ; Jump to new task
    ret  ; Use EIP on stack
    
load_user_task:
    ; Enhanced Ring 0->3 transition for user tasks
    
    ; Set up user mode stack
    mov esp, [ebx + 32]   ; User stack
    
    ; Push user mode context for IRET
    push 0x23             ; SS (user data selector)
    push dword [ebx + 32] ; ESP (user stack)
    push dword [ebx + 36] ; EFLAGS (with interrupts enabled)
    push 0x1B             ; CS (user code selector)  
    push dword [ebx + 28] ; EIP (user entry point)
    
    ; Load user data segments
    mov ax, 0x23          ; User data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Load general purpose registers
    mov eax, [ebx + 0]    ; eax
    mov ecx, [ebx + 8]    ; ecx
    mov edx, [ebx + 12]   ; edx
    mov esi, [ebx + 16]   ; esi
    mov edi, [ebx + 20]   ; edi
    mov ebp, [ebx + 24]   ; ebp
    mov ebx, [ebx + 4]    ; ebx (load last)
    
    ; Transition to Ring 3
    iret

context_switch_done:
    ; Restore stack and return
    mov esp, ebp
    pop ebp
    sti   ; Reactivate interrupts
    ret
