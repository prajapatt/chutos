#include "audio.h"

#include "console.h"
#include "ai.h"

static audio_input_state_t input_state;
static uint64_t frames_received;
static uint64_t voice_events;
static uint8_t initialized;
static uint8_t voice_latched;

static uint32_t sample_level(const int16_t *samples, uint32_t count)
{
    uint64_t total = 0;
    if (samples == 0 || count == 0)
    {
        return 0;
    }
    for (uint32_t index = 0; index < count; ++index)
    {
        int32_t sample = samples[index];
        total += (uint64_t)(sample < 0 ? -sample : sample);
    }
    return (uint32_t)(total / count);
}

void audio_init(void)
{
    input_state = AUDIO_INPUT_OFFLINE;
    frames_received = 0;
    voice_events = 0;
    voice_latched = 0;
    initialized = 1;
    console_write("audio: input pipeline ready for PCM16 device driver\n");
}

void audio_task(void)
{
    if (!initialized)
    {
        return;
    }
    if (input_state == AUDIO_INPUT_OFFLINE)
    {
        return;
    }
}

int audio_submit_pcm16(const int16_t *samples, uint32_t count)
{
    if (!initialized || samples == 0 || count == 0 || count > AUDIO_FRAME_SAMPLES)
    {
        return -1;
    }

    ++frames_received;
    uint32_t level = sample_level(samples, count);
    if (level >= AUDIO_VOICE_THRESHOLD)
    {
        input_state = AUDIO_INPUT_VOICE;
        if (!voice_latched)
        {
            voice_latched = 1;
            ++voice_events;
            ai_feed_voice_event();
        }
    }
    else
    {
        input_state = AUDIO_INPUT_READY;
        voice_latched = 0;
    }
    return 0;
}

audio_input_state_t audio_input_state(void)
{
    return input_state;
}

uint64_t audio_frames_received(void)
{
    return frames_received;
}

uint64_t audio_voice_events(void)
{
    return voice_events;
}
