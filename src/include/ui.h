#pragma once

#include <stdint.h>

void ui_init(uint64_t multiboot_info_address);
int ui_available(void);
void ui_status(const char *title, const char *value);
void ui_runtime_overlay(const char *mode, const char *app, const char *activity);
void ui_ai_conversation(const char *user_text, const char *response);
