#pragma once

#include <stdint.h>

void mouse_init(void);
void mouse_interrupt(void);
int mouse_present(void);
int32_t mouse_x(void);
int32_t mouse_y(void);
uint8_t mouse_buttons(void);
uint64_t mouse_packets(void);
