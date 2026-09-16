global long_mode_start
extern kernel_main
extern stack_top
extern tss64
extern gdt_tss_descriptor

section .text
bits 64
long_mode_start:
    xor eax, eax
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    mov word [gdt_tss_descriptor], 103
    mov rax, tss64
    mov word [gdt_tss_descriptor + 2], ax
    shr rax, 16
    mov byte [gdt_tss_descriptor + 4], al
    mov byte [gdt_tss_descriptor + 5], 0x89
    mov byte [gdt_tss_descriptor + 6], 0
    shr rax, 8
    mov byte [gdt_tss_descriptor + 7], al
    shr rax, 8
    mov dword [gdt_tss_descriptor + 8], eax
    mov qword [tss64 + 4], stack_top
    mov word [tss64 + 102], 104
    mov ax, 0x20
    ltr ax
    call kernel_main
.halt:
    cli
    hlt
    jmp .halt
