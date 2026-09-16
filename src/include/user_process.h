#pragma once

#include <stdint.h>

void user_process_prepare(void);
int user_process_ready(void);
void user_process_exit(uint64_t code);
int user_process_exited(void);
uint64_t user_process_exit_code(void);
uint64_t user_process_state(void);
uint64_t user_process_entry(void);
uint64_t user_process_stack(void);
