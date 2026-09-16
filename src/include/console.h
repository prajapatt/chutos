#pragma once

#include <stdint.h>

void console_init(void);
void console_clear(void);
void console_write_char(char character);
void console_write(const char *text);
void console_write_hex(uint64_t value);
void console_write_dec(uint64_t value);
