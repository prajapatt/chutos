global user_mode_enter

section .text
bits 64
user_mode_enter:
    cli
    push 0x13
    push rsi
    pushfq
    pop rax
    or rax, 0x200
    push rax
    push 0x1b
    push rdi
    iretq
