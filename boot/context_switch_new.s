[BITS 32]

section .text

global jump_to_task

; void jump_to_task(cpu_state_t* next_state)
; This function is called to start the first task.
; It does not save any state. It just loads the new state and iret's.
; This is how we enter user mode for the first time.

jump_to_task:
    mov eax, [esp + 4]  ; Get pointer to next_state

    ; Load the user data segment selector into ds, es, fs, gs
    mov ds, [eax + 48]  ; ds is at offset 48 in cpu_state_t
    mov es, [eax + 52]
    mov fs, [eax + 56]
    mov gs, [eax + 60]

    ; Push the IRET frame onto the stack from the cpu_state_t struct
    push dword [eax + 64] ; ss
    push dword [eax + 32] ; esp
    push dword [eax + 36] ; eflags
    push dword [eax + 44] ; cs is at offset 44
    push dword [eax + 28] ; eip

    ; Load general purpose registers
    mov ebp, [eax + 24]
    mov edi, [eax + 20]
    mov esi, [eax + 16]
    mov edx, [eax + 12]
    mov ecx, [eax + 8]
    mov ebx, [eax + 4]

    ; Load eax last, as we were using it
    mov eax, [eax + 0]

    ; Jump to user mode
    iret
