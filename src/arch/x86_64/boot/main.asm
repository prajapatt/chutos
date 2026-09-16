global stack_top
global tss64
global gdt_tss_descriptor

global start
extern long_mode_start

section .multiboot_header
align 8
header_start:
    dd 0xe85250d6
    dd 0
    dd header_end - header_start
    dd -(0xe85250d6 + (header_end - header_start))
    dw 0
    dw 0
    dd 8
header_end:

section .text
bits 32
start:
    mov esp, stack_top
    mov edi, ebx
    call check_multiboot
    call check_cpuid
    call check_long_mode
    call setup_page_tables
    call enable_paging
    lgdt [gdt64.pointer]
    jmp gdt64.code_segment:long_mode_start

check_multiboot:
    cmp eax, 0x36d76289
    jne .error_multiboot
    ret
.error_multiboot:
    mov al, 'M'
    jmp error

check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .error_cpuid
    ret
.error_cpuid:
    mov al, 'C'
    jmp error

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .error_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .error_long_mode
    ret
.error_long_mode:
    mov al, 'L'
    jmp error

setup_page_tables:
    mov eax, page_table_l3
    or eax, 0b11
    mov [page_table_l4], eax
    mov eax, page_table_l2
    or eax, 0b11
    mov [page_table_l3], eax
    xor ecx, ecx
.map:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011
    mov [page_table_l2 + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map
    ret

enable_paging:
    mov eax, page_table_l4
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov ecx, 0xc0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    ret

error:
    mov dword [0xb8000], 0x4f524f45
    mov dword [0xb8004], 0x4f3a4f52
    mov byte [0xb800a], al
.halt:
    cli
    hlt
    jmp .halt

section .bss
align 4096
page_table_l4: resb 4096
page_table_l3: resb 4096
page_table_l2: resb 4096
stack_bottom: resb 4096 * 8
stack_top:
align 16
tss64: resb 104

section .rodata
gdt64:
    dq 0
.code_segment equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
.user_data_segment equ $ - gdt64
    dq 0x00cff2000000ffff
.user_code_segment equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 45) | (1 << 46) | (1 << 47) | (1 << 53)
.tss_segment equ $ - gdt64
gdt_tss_descriptor: times 16 db 0
.pointer:
    dw $ - gdt64 - 1
    dq gdt64
