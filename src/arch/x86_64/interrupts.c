#include "interrupts.h"
#include "console.h"
#include "keyboard.h"
#include "memory.h"
#include "mouse.h"
#include "port.h"
#include "scheduler.h"
#include "user_process.h"

#define IDT_ENTRIES 256
#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_COMMAND 0xa0
#define PIC_SLAVE_DATA 0xa1
#define PIC_EOI 0x20
#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40

typedef struct __attribute__((packed))
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} idt_entry_t;

typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint64_t base;
} idtr_t;

extern void interrupt_timer_stub(void);
extern void interrupt_default_stub(void);
extern void interrupt_keyboard_stub(void);
extern void interrupt_mouse_stub(void);
extern void interrupt_syscall_stub(void);
extern void interrupt_exception_0(void);
extern void interrupt_exception_1(void);
extern void interrupt_exception_2(void);
extern void interrupt_exception_3(void);
extern void interrupt_exception_4(void);
extern void interrupt_exception_5(void);
extern void interrupt_exception_6(void);
extern void interrupt_exception_7(void);
extern void interrupt_exception_8(void);
extern void interrupt_exception_9(void);
extern void interrupt_exception_10(void);
extern void interrupt_exception_11(void);
extern void interrupt_exception_12(void);
extern void interrupt_exception_13(void);
extern void interrupt_exception_14(void);
extern void interrupt_exception_15(void);
extern void interrupt_exception_16(void);
extern void interrupt_exception_17(void);
extern void interrupt_exception_18(void);
extern void interrupt_exception_19(void);
extern void interrupt_exception_20(void);
extern void interrupt_exception_21(void);
extern void interrupt_exception_22(void);
extern void interrupt_exception_23(void);
extern void interrupt_exception_24(void);
extern void interrupt_exception_25(void);
extern void interrupt_exception_26(void);
extern void interrupt_exception_27(void);
extern void interrupt_exception_28(void);
extern void interrupt_exception_29(void);
extern void interrupt_exception_30(void);
extern void interrupt_exception_31(void);
static idt_entry_t idt[IDT_ENTRIES];
static uint64_t syscall_count;

static void (*const exception_handlers[32])(void) = {
    interrupt_exception_0,
    interrupt_exception_1,
    interrupt_exception_2,
    interrupt_exception_3,
    interrupt_exception_4,
    interrupt_exception_5,
    interrupt_exception_6,
    interrupt_exception_7,
    interrupt_exception_8,
    interrupt_exception_9,
    interrupt_exception_10,
    interrupt_exception_11,
    interrupt_exception_12,
    interrupt_exception_13,
    interrupt_exception_14,
    interrupt_exception_15,
    interrupt_exception_16,
    interrupt_exception_17,
    interrupt_exception_18,
    interrupt_exception_19,
    interrupt_exception_20,
    interrupt_exception_21,
    interrupt_exception_22,
    interrupt_exception_23,
    interrupt_exception_24,
    interrupt_exception_25,
    interrupt_exception_26,
    interrupt_exception_27,
    interrupt_exception_28,
    interrupt_exception_29,
    interrupt_exception_30,
    interrupt_exception_31,
};

static void idt_set_gate(uint8_t vector, void (*handler)(void), uint8_t attributes)
{
    uint64_t address = (uint64_t)handler;
    idt[vector] = (idt_entry_t){
        (uint16_t)address,
        0x08,
        0,
        attributes,
        (uint16_t)(address >> 16),
        (uint32_t)(address >> 32),
        0,
    };
}

static void pic_remap(void)
{
    uint8_t master_mask = inb(PIC_MASTER_DATA);
    uint8_t slave_mask = inb(PIC_SLAVE_DATA);
    outb(PIC_MASTER_COMMAND, 0x11);
    io_wait();
    outb(PIC_SLAVE_COMMAND, 0x11);
    io_wait();
    outb(PIC_MASTER_DATA, 0x20);
    io_wait();
    outb(PIC_SLAVE_DATA, 0x28);
    io_wait();
    outb(PIC_MASTER_DATA, 0x04);
    io_wait();
    outb(PIC_SLAVE_DATA, 0x02);
    io_wait();
    outb(PIC_MASTER_DATA, 0x01);
    io_wait();
    outb(PIC_SLAVE_DATA, 0x01);
    io_wait();
    outb(PIC_MASTER_DATA, master_mask & (uint8_t)~0x07);
    outb(PIC_SLAVE_DATA, slave_mask & (uint8_t)~0x10);
}

static void pit_init(void)
{
    const uint16_t divisor = 1193182 / 100;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xff);
    outb(PIT_CHANNEL0, divisor >> 8);
}

void interrupts_init(void)
{
    for (uint16_t vector = 0; vector < IDT_ENTRIES; ++vector)
    {
        idt_set_gate((uint8_t)vector, interrupt_default_stub, 0x8e);
    }
    for (uint8_t vector = 0; vector < 32; ++vector)
    {
        idt_set_gate(vector, exception_handlers[vector], 0x8e);
    }
    idt_set_gate(32, interrupt_timer_stub, 0x8e);
    idt_set_gate(33, interrupt_keyboard_stub, 0x8e);
    idt_set_gate(44, interrupt_mouse_stub, 0x8e);
    idt_set_gate(0x80, interrupt_syscall_stub, 0xee);
    pic_remap();
    pit_init();
    idtr_t descriptor = {(uint16_t)(sizeof(idt) - 1), (uint64_t)idt};
    __asm__ volatile("lidt %0" : : "m"(descriptor));
}

void interrupts_enable(void)
{
    __asm__ volatile("sti");
}

void interrupts_disable(void)
{
    __asm__ volatile("cli");
}

uint64_t interrupt_syscall_dispatch(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                                    uint64_t arg4, uint64_t arg5)
{
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    ++syscall_count;

    if (user_process_exited() && number != SYSCALL_GET_PROCESS_STATE)
    {
        return UINT64_MAX;
    }

    if (number == SYSCALL_GET_TICKS)
    {
        return scheduler_ticks();
    }
    if (number == SYSCALL_GET_COUNT)
    {
        return syscall_count;
    }
    if (number == SYSCALL_YIELD)
    {
        return 0;
    }
    if (number == SYSCALL_EXIT)
    {
        user_process_exit(arg1);
        return 0;
    }
    if (number == SYSCALL_GET_MEMORY_FREE)
    {
        return memory_pages_free();
    }
    if (number == SYSCALL_GET_PROCESS_STATE)
    {
        return user_process_state();
    }
    return UINT64_MAX;
}

uint64_t interrupt_syscall_count(void)
{
    return syscall_count;
}

void interrupt_dispatch(uint64_t vector)
{
    if (vector < 32)
    {
        interrupts_disable();
        console_write("\n[chutos] fatal CPU exception vector ");
        console_write_dec(vector);
        console_write("\n");
        for (;;)
        {
            __asm__ volatile("hlt");
        }
    }
    else if (vector == 32)
    {
        scheduler_tick();
        outb(PIC_MASTER_COMMAND, PIC_EOI);
    }
    else if (vector == 33)
    {
        keyboard_interrupt();
        outb(PIC_MASTER_COMMAND, PIC_EOI);
    }
    else if (vector == 44)
    {
        mouse_interrupt();
        outb(PIC_SLAVE_COMMAND, PIC_EOI);
        outb(PIC_MASTER_COMMAND, PIC_EOI);
    }
    else if (vector == 0x80)
    {
        ++syscall_count;
    }
}
