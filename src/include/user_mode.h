#pragma once

#include <stdint.h>

#define USER_MODE_CODE_SELECTOR 0x1b
#define USER_MODE_DATA_SELECTOR 0x13

void user_mode_enter(uint64_t entry_point, uint64_t stack_pointer);
