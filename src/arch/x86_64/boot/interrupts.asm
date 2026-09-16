global interrupt_timer_stub
global interrupt_keyboard_stub
global interrupt_mouse_stub
global interrupt_syscall_stub
global interrupt_default_stub
%assign vector 0
%rep 32
global interrupt_exception_%+vector
%assign vector vector + 1
%endrep
extern interrupt_dispatch
extern interrupt_syscall_dispatch

section .text
interrupt_timer_stub:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push 32
    call interrupt_dispatch
    add rsp, 8
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

interrupt_keyboard_stub:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push 33
    call interrupt_dispatch
    add rsp, 8
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

interrupt_mouse_stub:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push 44
    call interrupt_dispatch
    add rsp, 8
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

interrupt_syscall_stub:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push 128
    mov rdi, [rsp + 48]
    mov rsi, [rsp + 56]
    mov rdx, [rsp + 64]
    mov rcx, [rsp + 16]
    mov r8, [rsp + 32]
    mov r9, [rsp + 24]
    call interrupt_syscall_dispatch
    mov [rsp + 80], rax
    add rsp, 8
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

interrupt_default_stub:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push 255
    call interrupt_dispatch
    add rsp, 8
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

%macro EXCEPTION_NO_ERROR 1
interrupt_exception_%1:
    push %1
    jmp exception_no_error_common
%endmacro

%macro EXCEPTION_ERROR 1
interrupt_exception_%1:
    push %1
    jmp exception_error_common
%endmacro

exception_no_error_common:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    call interrupt_dispatch
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    add rsp, 8
    iretq

exception_error_common:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    call interrupt_dispatch
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    add rsp, 16
    iretq

EXCEPTION_NO_ERROR 0
EXCEPTION_NO_ERROR 1
EXCEPTION_NO_ERROR 2
EXCEPTION_NO_ERROR 3
EXCEPTION_NO_ERROR 4
EXCEPTION_NO_ERROR 5
EXCEPTION_NO_ERROR 6
EXCEPTION_NO_ERROR 7
EXCEPTION_ERROR 8
EXCEPTION_NO_ERROR 9
EXCEPTION_ERROR 10
EXCEPTION_ERROR 11
EXCEPTION_ERROR 12
EXCEPTION_ERROR 13
EXCEPTION_ERROR 14
EXCEPTION_NO_ERROR 15
EXCEPTION_NO_ERROR 16
EXCEPTION_ERROR 17
EXCEPTION_NO_ERROR 18
EXCEPTION_NO_ERROR 19
EXCEPTION_NO_ERROR 20
EXCEPTION_NO_ERROR 21
EXCEPTION_NO_ERROR 22
EXCEPTION_NO_ERROR 23
EXCEPTION_NO_ERROR 24
EXCEPTION_NO_ERROR 25
EXCEPTION_NO_ERROR 26
EXCEPTION_NO_ERROR 27
EXCEPTION_NO_ERROR 28
EXCEPTION_NO_ERROR 29
EXCEPTION_ERROR 30
EXCEPTION_NO_ERROR 31
