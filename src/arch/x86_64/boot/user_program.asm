global user_program_start

global user_program_end

section .text
bits 64
user_program_start:
    mov eax, 0
    int 0x80
    mov eax, 2
    int 0x80
    mov eax, 4
    int 0x80
    mov eax, 5
    int 0x80
    jmp user_program_start
user_program_end:
