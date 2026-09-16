#pragma once

#include <stdint.h>

void ai_init(void);
void ai_task(void);
void ai_feed_key(char character);
void ai_feed_voice_event(void);
const char *ai_status_line(void);
const char *ai_last_command(void);
const char *ai_response_line(void);
