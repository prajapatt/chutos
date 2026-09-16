#pragma once

#include <stdint.h>

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_FRAME_SAMPLES 160
#define AUDIO_VOICE_THRESHOLD 900

typedef enum
{
    AUDIO_INPUT_OFFLINE = 0,
    AUDIO_INPUT_READY = 1,
    AUDIO_INPUT_VOICE = 2
} audio_input_state_t;

void audio_init(void);
void audio_task(void);
int audio_submit_pcm16(const int16_t *samples, uint32_t count);
audio_input_state_t audio_input_state(void);
uint64_t audio_frames_received(void);
uint64_t audio_voice_events(void);
