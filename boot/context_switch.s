; Enhanced context switching with Ring 0->3 transitions for AI-OS
[BITS 32]

section .text

global context_switch_with_ring_transition
global switch_to_user_mode

extern vmm_switch_page_directory

; Segment selectors
KERNEL_CODE_SELECTOR equ 0x08
KERNEL_DATA_SELECTOR equ 0x10
USER_CODE_SELECTOR   equ 0x1B  ; Ring 3 code
USER_DATA_SELECTOR   equ 0x23  ; Ring 3 data

; Context switch with Ring transition support
context_switch_with_ring_transition:
    cli
    
    ; Get parameters
    mov eax, [esp + 4]  ; prev_task
    mov edx, [esp + 8]  ; next_task
    
    ; Save current task context if exists
    test eax, eax
    jz .load_next
    
    ; Save registers
    mov [eax + 20], esp
    mov [eax + 24], ebp
    mov [eax + 28], ebx
    mov [eax + 32], esi
    mov [eax + 36], edi
    
.load_next:
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
    call vmm_switch_page_directory
    add esp, 4
    pop edx
    
    ; Set up user mode transition
    mov esp, [edx + 20]  ; User stack
    mov ebx, [edx + 56]  ; Entry point
    
    ; Push user mode context for iret
    push USER_DATA_SELECTOR  ; SS
    push esp                 ; ESP
    pushfd                   ; EFLAGS
    pop ecx
    or ecx, 0x200           ; Enable interrupts
    push ecx                ; EFLAGS
    push USER_CODE_SELECTOR ; CS
    push ebx                ; EIP
    
    ; Load user segments
    mov ax, USER_DATA_SELECTOR
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
    call vmm_switch_page_directory
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