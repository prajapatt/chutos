ENTRY(start)

SECTIONS
{
    . = 1M;
    .boot : ALIGN(8)
    {
        KEEP(*(.multiboot_header))
    }
    .text : ALIGN(16)
    {
        *(.text*)
    }
    .rodata : ALIGN(16)
    {
        *(.rodata*)
    }
    .data : ALIGN(16)
    {
        *(.data*)
    }
    .bss : ALIGN(16)
    {
        *(COMMON)
        *(.bss*)
    }

    __kernel_end = .;
}
