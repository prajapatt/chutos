#include "user_process.h"

#include "console.h"
#include "memory.h"
#include "paging.h"

#define USER_PROCESS_STACK_TOP (PAGING_USER_BASE + PAGING_USER_WINDOW_SIZE)
#define USER_PROCESS_STACK_PAGE (USER_PROCESS_STACK_TOP - 4096ULL)

extern uint8_t user_program_start;
extern uint8_t user_program_end;

static uint8_t prepared;
static uint8_t exited;
static uint64_t exit_code;
static uint64_t entry_address;
static uint64_t stack_address;

enum
{
    USER_PROCESS_UNPREPARED = 0,
    USER_PROCESS_READY = 1,
    USER_PROCESS_EXITED = 2
};

void user_process_prepare(void)
{
    if (prepared)
    {
        return;
    }

    paging_address_space_t *address_space = paging_create_address_space();
    uint8_t *code_page = (uint8_t *)memory_alloc_page();
    uint8_t *stack_page = (uint8_t *)memory_alloc_page();
    uint64_t program_size = (uint64_t)(uintptr_t)&user_program_end -
                            (uint64_t)(uintptr_t)&user_program_start;
    if (address_space == 0 || code_page == 0 || stack_page == 0 || program_size == 0 || program_size > 4096)
    {
        console_write("process: user image preparation failed\n");
        return;
    }

    for (uint64_t index = 0; index < program_size; ++index)
    {
        code_page[index] = (&user_program_start)[index];
    }

    if (paging_map_address_space_page(address_space, PAGING_USER_BASE, (uint64_t)(uintptr_t)code_page) != 0 ||
        paging_map_address_space_page(address_space, USER_PROCESS_STACK_PAGE,
                                      (uint64_t)(uintptr_t)stack_page) != 0)
    {
        console_write("process: user image mapping failed\n");
        return;
    }

    entry_address = PAGING_USER_BASE;
    stack_address = USER_PROCESS_STACK_TOP;
    prepared = 1;
    console_write("process: user image prepared, code bytes=");
    console_write_dec(program_size);
    console_write("\n");
}

int user_process_ready(void)
{
    return prepared != 0;
}

void user_process_exit(uint64_t code)
{
    if (exited)
    {
        return;
    }
    exited = 1;
    exit_code = code;
    console_write("process: user process exit code=");
    console_write_dec(code);
    console_write("\n");
}

int user_process_exited(void)
{
    return exited != 0;
}

uint64_t user_process_exit_code(void)
{
    return exit_code;
}

uint64_t user_process_state(void)
{
    if (exited)
    {
        return USER_PROCESS_EXITED;
    }
    return prepared ? USER_PROCESS_READY : USER_PROCESS_UNPREPARED;
}

uint64_t user_process_entry(void)
{
    return entry_address;
}

uint64_t user_process_stack(void)
{
    return stack_address;
}
