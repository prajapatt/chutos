#pragma once

#include <stdint.h>

#define DESKTOP_MAX_WINDOWS 6

typedef struct
{
    uint8_t active;
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
    const char *title;
} desktop_window_t;

void desktop_init(void);
void desktop_handle_key(char character);
void desktop_tick(void);
const char *desktop_active_app(void);
const char *desktop_mode_name(void);
void desktop_set_mode(const char *mode);
void desktop_handle_voice_intent(const char *intent);
void desktop_launch_window(const char *title, uint8_t x, uint8_t y, uint8_t width, uint8_t height);
void desktop_focus_next(void);
void desktop_focus_previous(void);
void desktop_run_active_app(void);
void desktop_refresh_runtime_panel(void);
