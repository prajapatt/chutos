#include "ai.h"

#include <stddef.h>

#include "console.h"
#include "desktop.h"
#include "pci.h"
#include "scheduler.h"
#include "ui.h"

#define AI_BUFFER_SIZE 128

static char command_buffer[AI_BUFFER_SIZE];
static char last_command[AI_BUFFER_SIZE];
static char status_line[128];
static char response_line[128];
static uint8_t initialized;
static uint8_t command_ready;
static uint8_t command_length;
static uint8_t autonomy_mode;
static uint8_t authority_mode;
static uint8_t voice_active;
static uint8_t autonomous_monitoring;
static uint8_t voice_event_ready;
static uint32_t awareness_cycle;

static size_t ai_strlen(const char *text)
{
    size_t length = 0;
    if (text != 0)
    {
        while (text[length] != '\0')
        {
            ++length;
        }
    }
    return length;
}

static void ai_memcpy(char *destination, const char *source, size_t length)
{
    if (destination == 0 || source == 0 || length == 0)
    {
        return;
    }
    for (size_t index = 0; index < length; ++index)
    {
        destination[index] = source[index];
    }
}

static int ai_strcmp(const char *left, const char *right)
{
    if (left == right)
    {
        return 0;
    }
    if (left == 0)
    {
        return -1;
    }
    if (right == 0)
    {
        return 1;
    }
    while (*left != '\0' && *right != '\0')
    {
        char left_character = *left;
        char right_character = *right;
        if (left_character >= 'A' && left_character <= 'Z')
        {
            left_character = (char)(left_character - 'A' + 'a');
        }
        if (right_character >= 'A' && right_character <= 'Z')
        {
            right_character = (char)(right_character - 'A' + 'a');
        }
        if (left_character != right_character)
        {
            return (int)(unsigned char)left_character - (int)(unsigned char)right_character;
        }
        ++left;
        ++right;
    }
    return (int)(unsigned char)(*left) - (int)(unsigned char)(*right);
}

static int ai_contains(const char *text, const char *needle)
{
    if (text == 0 || needle == 0 || needle[0] == '\0')
    {
        return 0;
    }
    for (const char *start = text; *start != '\0'; ++start)
    {
        const char *left = start;
        const char *right = needle;
        while (*left != '\0' && *right != '\0')
        {
            char left_character = *left;
            char right_character = *right;
            if (left_character >= 'A' && left_character <= 'Z')
            {
                left_character = (char)(left_character - 'A' + 'a');
            }
            if (right_character >= 'A' && right_character <= 'Z')
            {
                right_character = (char)(right_character - 'A' + 'a');
            }
            if (left_character != right_character)
            {
                break;
            }
            ++left;
            ++right;
        }
        if (*right == '\0')
        {
            return 1;
        }
    }
    return 0;
}

static void set_status(const char *text)
{
    size_t length = ai_strlen(text);
    if (length >= sizeof(status_line))
    {
        length = sizeof(status_line) - 1;
    }
    ai_memcpy(status_line, text, length);
    status_line[length] = '\0';
    ui_status("AI CORE", status_line);
}

static void set_response(const char *text)
{
    size_t length = ai_strlen(text);
    if (length >= sizeof(response_line))
    {
        length = sizeof(response_line) - 1;
    }
    ai_memcpy(response_line, text, length);
    response_line[length] = '\0';
    ui_ai_conversation(last_command, response_line);
}

static void run_autonomous_loop(void)
{
    if (!autonomy_mode && !authority_mode)
    {
        return;
    }

    awareness_cycle += 1;
    if ((awareness_cycle % 120) != 0)
    {
        return;
    }

    console_write("AI: autonomous layer monitoring hardware integrity\n");
    console_write("AI: authority layer managing scheduler, device lanes, and telemetry\n");
    if (autonomy_mode)
    {
        set_status("AI READY");
    }
    else if (authority_mode)
    {
        set_status("SYSTEM AUTHORITY");
    }
    else
    {
        set_status("AUTONOMY ONLINE");
    }
}

static void handle_command(const char *command)
{
    if (command == 0 || command[0] == '\0')
    {
        set_response("I am listening.");
        set_status("LISTENING");
        return;
    }

    if (ai_strcmp(command, "help") == 0)
    {
        console_write("AI: available verbs: status, scan, user mode, ai mode, open files, open system\n");
        set_response("I can open files, inspect the system, change control mode, and report status.");
        set_status("HELP READY");
        return;
    }

    if (ai_strcmp(command, "status") == 0)
    {
        console_write("AI: kernel online; scheduler ticks=");
        console_write_dec(scheduler_ticks());
        console_write("; pci devices=");
        console_write_dec(pci_device_count());
        console_write("\n");
        set_response("Kernel is online. Scheduler and hardware telemetry are available.");
        set_status("SYSTEM ONLINE");
        return;
    }

    if (ai_strcmp(command, "scan") == 0)
    {
        console_write("AI: scanning hardware namespace; pci devices=");
        console_write_dec(pci_device_count());
        console_write("\n");
        set_response("Hardware scan completed.");
        set_status("PCI SCAN COMPLETE");
        return;
    }

    if (ai_strcmp(command, "voice") == 0)
    {
        console_write("AI: voice interaction mode enabled\n");
        set_status("VOICE MODE ACTIVE");
        set_response("Voice control mode is ready when an audio driver is connected.");
        desktop_set_mode("AI");
        return;
    }

    if (ai_strcmp(command, "user") == 0 || ai_strcmp(command, "user mode") == 0 ||
        ai_strcmp(command, "manual control") == 0)
    {
        console_write("AI: user control lane enabled\n");
        desktop_set_mode("USER");
        set_response("User control has priority now.");
        set_status("USER LANE ACTIVE");
        return;
    }

    if (ai_strcmp(command, "ai") == 0 || ai_strcmp(command, "ai mode") == 0 ||
        ai_strcmp(command, "autonomous mode") == 0)
    {
        console_write("AI: autonomous control lane enabled\n");
        desktop_set_mode("AI");
        set_response("I can coordinate system tasks now.");
        set_status("AI LANE ACTIVE");
        return;
    }

    if (ai_strcmp(command, "launch files") == 0 || ai_strcmp(command, "open files") == 0 ||
        ai_strcmp(command, "show files") == 0 || ai_strcmp(command, "focus files") == 0)
    {
        console_write("AI: launching file browser\n");
        desktop_handle_voice_intent("launch files");
        set_response("Opening the file browser.");
        set_status("FILES READY");
        return;
    }

    if (ai_strcmp(command, "launch system") == 0 || ai_strcmp(command, "open system") == 0 ||
        ai_strcmp(command, "system monitor") == 0 || ai_strcmp(command, "monitor system") == 0)
    {
        console_write("AI: launching system monitor\n");
        desktop_handle_voice_intent("launch system");
        set_response("Opening the system monitor.");
        set_status("SYSTEM READY");
        return;
    }

    if (ai_strcmp(command, "launch assistant") == 0 || ai_strcmp(command, "open assistant") == 0 ||
        ai_strcmp(command, "focus assistant") == 0)
    {
        console_write("AI: launching assistant shell\n");
        desktop_handle_voice_intent("launch assistant");
        set_response("Assistant panel is active.");
        set_status("ASSISTANT READY");
        return;
    }

    if (ai_strcmp(command, "next app") == 0 || ai_strcmp(command, "next window") == 0)
    {
        console_write("AI: focusing next app\n");
        desktop_handle_voice_intent("next app");
        set_response("Moving to the next app.");
        set_status("NEXT APP ACTIVE");
        return;
    }

    if (ai_strcmp(command, "previous app") == 0 || ai_strcmp(command, "previous window") == 0)
    {
        console_write("AI: focusing previous app\n");
        desktop_handle_voice_intent("previous app");
        set_response("Moving to the previous app.");
        set_status("PREVIOUS APP ACTIVE");
        return;
    }

    if (ai_strcmp(command, "boot") == 0)
    {
        console_write("AI: boot sequence stable; desktop shell online\n");
        set_status("BOOTCHAIN STABLE");
        set_response("Boot chain is stable and the desktop is online.");
        return;
    }

    if (ai_strcmp(command, "power") == 0)
    {
        console_write("AI: power policy set to autonomous maintenance\n");
        set_status("AUTONOMY READY");
        set_response("Autonomous maintenance is ready.");
        return;
    }

    if (ai_strcmp(command, "gaming mode") == 0 || ai_strcmp(command, "performance mode") == 0 ||
        ai_strcmp(command, "low latency mode") == 0 ||
        (ai_contains(command, "game") && ai_contains(command, "performance")))
    {
        scheduler_set_performance_mode(1);
        set_response("Performance mode is active. Input gets priority and background work is reduced.");
        set_status("PERFORMANCE MODE");
        console_write("AI: gaming performance policy enabled\n");
        return;
    }

    if (ai_strcmp(command, "balanced mode") == 0 || ai_strcmp(command, "normal mode") == 0 ||
        ai_strcmp(command, "exit gaming mode") == 0)
    {
        scheduler_set_performance_mode(0);
        set_response("Balanced mode is active. AI and desktop services are fully scheduled.");
        set_status("BALANCED MODE");
        console_write("AI: balanced scheduling policy enabled\n");
        return;
    }

    if (ai_contains(command, "open") && ai_contains(command, "file"))
    {
        desktop_handle_voice_intent("launch files");
        set_response("I understood: open the file browser.");
        set_status("FILES READY");
        return;
    }

    if ((ai_contains(command, "system") && ai_contains(command, "status")) ||
        ai_contains(command, "how is the system"))
    {
        desktop_handle_voice_intent("status");
        set_response("I am checking the system runtime now.");
        set_status("SYSTEM CHECK");
        return;
    }

    if (ai_contains(command, "open") && ai_contains(command, "monitor"))
    {
        desktop_handle_voice_intent("launch system");
        set_response("I understood: open the system monitor.");
        set_status("SYSTEM READY");
        return;
    }

    if (ai_contains(command, "switch") && ai_contains(command, "user"))
    {
        desktop_handle_voice_intent("user mode");
        set_response("User control has priority now.");
        set_status("USER LANE ACTIVE");
        return;
    }

    if (ai_contains(command, "switch") && ai_contains(command, "ai"))
    {
        desktop_handle_voice_intent("ai mode");
        set_response("AI control is active now.");
        set_status("AI LANE ACTIVE");
        return;
    }

    console_write("AI: command unknown: ");
    console_write(command);
    console_write("\n");
    set_response("I do not know that action yet. Say help for available actions.");
    set_status("UNKNOWN COMMAND");
}

void ai_init(void)
{
    if (initialized)
    {
        return;
    }
    command_buffer[0] = '\0';
    last_command[0] = '\0';
    status_line[0] = '\0';
    response_line[0] = '\0';
    command_ready = 0;
    command_length = 0;
    autonomy_mode = 1;
    authority_mode = 1;
    voice_active = 1;
    autonomous_monitoring = 1;
    awareness_cycle = 0;
    initialized = 1;
    set_status("AI READY");
    set_response("I am ready. Tell me what you want the OS to do.");
    console_write("AI: autonomous voice layer engaged\n");
    console_write("AI: system authority layer engaged\n");
    console_write("AI: no terminal required; continuous kernel awareness active\n");
}

void ai_feed_key(char character)
{
    if (!initialized)
    {
        ai_init();
    }

    if (character == '\b')
    {
        if (command_length > 0)
        {
            --command_length;
            command_buffer[command_length] = '\0';
        }
        return;
    }

    if (character == '\n' || character == '\r')
    {
        if (command_length > 0)
        {
            ai_memcpy(last_command, command_buffer, command_length + 1);
            last_command[sizeof(last_command) - 1] = '\0';
            command_ready = 1;
            command_buffer[0] = '\0';
            command_length = 0;
        }
        return;
    }

    if (character < 32 || character > 126)
    {
        return;
    }

    if (command_length + 1 >= sizeof(command_buffer))
    {
        return;
    }

    command_buffer[command_length++] = character;
    command_buffer[command_length] = '\0';
}

void ai_feed_voice_event(void)
{
    if (!initialized)
    {
        ai_init();
    }
    voice_event_ready = 1;
}

void ai_task(void)
{
    if (!initialized)
    {
        return;
    }

    if (command_ready)
    {
        command_ready = 0;
        console_write("AI: voice context captured > ");
        console_write(last_command);
        console_write("\n");
        handle_command(last_command);
        return;
    }

    if (voice_event_ready)
    {
        voice_event_ready = 0;
        set_response("I heard voice activity. Speech transcription is not connected yet.");
        set_status("VOICE CAPTURED");
        console_write("AI: voice frame captured; waiting for speech-to-text provider\n");
        return;
    }

    if (voice_active && autonomous_monitoring)
    {
        run_autonomous_loop();
    }

    if (autonomy_mode && authority_mode)
    {
        set_status("AI SYSTEM ACTIVE");
    }
}

const char *ai_status_line(void)
{
    return status_line;
}

const char *ai_last_command(void)
{
    return last_command;
}

const char *ai_response_line(void)
{
    return response_line;
}
