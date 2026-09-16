#pragma once

#include <stdint.h>

#define SYSCALL_GET_TICKS 0
#define SYSCALL_GET_COUNT 1
#define SYSCALL_YIELD 2
#define SYSCALL_EXIT 3
#define SYSCALL_GET_MEMORY_FREE 4
#define SYSCALL_GET_PROCESS_STATE 5

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
void interrupt_dispatch(uint64_t vector);
uint64_t interrupt_syscall_dispatch(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                                    uint64_t arg4, uint64_t arg5);
uint64_t interrupt_syscall_count(void);
