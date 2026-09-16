#include "desktop.h"

#include "console.h"
#include "audio.h"
#include "fat.h"
#include "gpu.h"
#include "interrupts.h"
#include "memory.h"
#include "mouse.h"
#include "paging.h"
#include "pci.h"
#include "scheduler.h"
#include "ui.h"
#include "user_process.h"
#include "xhci.h"

#define DESKTOP_APP_COUNT 4

static const char *apps[DESKTOP_APP_COUNT] = {
    "ASSISTANT",
    "FILES",
    "NETWORK",
    "SYSTEM",
};

static desktop_window_t windows[DESKTOP_MAX_WINDOWS];
static uint8_t initialized;
static uint8_t active_index;
static uint8_t window_count;
static char last_key;
static char mode_name[16];
static uint32_t tick_count;
static uint32_t file_browser_index;
static uint8_t runtime_panel_dirty;
static uint8_t file_preview[65];

static int strings_equal(const char *left, const char *right);
static void set_active_app(uint8_t index);
static void refresh_window_state(void);

static void focus_window_by_title(const char *title)
{
    if (!initialized || title == 0)
    {
        return;
    }

    for (uint8_t index = 0; index < window_count; ++index)
    {
        if (windows[index].title != 0 && strings_equal(windows[index].title, title))
        {
            active_index = index;
            set_active_app(active_index);
            refresh_window_state();
            desktop_refresh_runtime_panel();
            return;
        }
    }
}

void desktop_set_mode(const char *mode)
{
    const char *value = mode;
    if (value == 0 || value[0] == '\0')
    {
        value = "HYBRID";
    }

    if (strings_equal(value, "USER") || strings_equal(value, "USER MODE") ||
        strings_equal(value, "MANUAL") || strings_equal(value, "DIRECT"))
    {
        value = "USER";
    }
    else if (strings_equal(value, "AI") || strings_equal(value, "AI MODE") ||
             strings_equal(value, "AUTONOMY") || strings_equal(value, "VOICE"))
    {
        value = "AI";
    }
    else
    {
        value = "HYBRID";
    }

    for (uint8_t index = 0; index < sizeof(mode_name) - 1; ++index)
    {
        mode_name[index] = '\0';
    }

    uint8_t index = 0;
    while (value[index] != '\0' && index < sizeof(mode_name) - 1)
    {
        mode_name[index] = value[index];
        ++index;
    }
    mode_name[index] = '\0';

    ui_status("MODE", mode_name);
    console_write("desktop: mode -> ");
    console_write(mode_name);
    console_write("\n");
}

const char *desktop_mode_name(void)
{
    return mode_name[0] != '\0' ? mode_name : "HYBRID";
}

void desktop_handle_voice_intent(const char *intent)
{
    if (intent == 0 || intent[0] == '\0')
    {
        return;
    }

    if (strings_equal(intent, "user") || strings_equal(intent, "user mode") ||
        strings_equal(intent, "manual") || strings_equal(intent, "direct control"))
    {
        desktop_set_mode("USER");
        return;
    }

    if (strings_equal(intent, "ai") || strings_equal(intent, "ai mode") ||
        strings_equal(intent, "autonomy") || strings_equal(intent, "voice mode"))
    {
        desktop_set_mode("AI");
        return;
    }

    if (strings_equal(intent, "launch files") || strings_equal(intent, "open files") ||
        strings_equal(intent, "files"))
    {
        focus_window_by_title("FILES");
        return;
    }

    if (strings_equal(intent, "launch assistant") || strings_equal(intent, "open assistant") ||
        strings_equal(intent, "assistant"))
    {
        focus_window_by_title("ASSISTANT");
        return;
    }

    if (strings_equal(intent, "launch system") || strings_equal(intent, "open system") ||
        strings_equal(intent, "system"))
    {
        focus_window_by_title("SYSTEM");
        return;
    }

    if (strings_equal(intent, "launch network") || strings_equal(intent, "open network") ||
        strings_equal(intent, "network"))
    {
        focus_window_by_title("NETWORK");
        return;
    }

    if (strings_equal(intent, "status") || strings_equal(intent, "status check"))
    {
        desktop_refresh_runtime_panel();
        return;
    }

    if (strings_equal(intent, "next app") || strings_equal(intent, "next window"))
    {
        desktop_focus_next();
        return;
    }

    if (strings_equal(intent, "previous app") || strings_equal(intent, "previous window"))
    {
        desktop_focus_previous();
    }
}

static int strings_equal(const char *left, const char *right)
{
    if (left == 0 || right == 0)
    {
        return left == right;
    }
    while (*left != '\0' && *right != '\0' && *left == *right)
    {
        ++left;
        ++right;
    }
    return *left == *right;
}

static void set_active_app(uint8_t index)
{
    if (index >= DESKTOP_APP_COUNT)
    {
        index = 0;
    }
    active_index = index;
    ui_status("APP", apps[active_index]);
    console_write("desktop: active app -> ");
    console_write(apps[active_index]);
    console_write("\n");
}

static void refresh_window_state(void)
{
    for (uint8_t index = 0; index < window_count; ++index)
    {
        windows[index].active = (index == active_index) ? 1 : 0;
    }
}

void desktop_refresh_runtime_panel(void)
{
    if (!initialized)
    {
        return;
    }
    runtime_panel_dirty = 1;
    ui_status("APP", desktop_active_app());
    ui_runtime_overlay(desktop_mode_name(), desktop_active_app(), "TASK QUEUED");
    console_write("desktop: runtime panel refreshed for ");
    console_write(desktop_active_app());
    console_write("\n");
}

static void desktop_file_browser_loop(void)
{
    if (!fat32_mounted())
    {
        console_write("desktop: file browser offline -> no mounted filesystem\n");
        return;
    }

    fat32_root_entry_t entry;
    if (fat32_root_entry(file_browser_index, &entry) != 0)
    {
        file_browser_index = 0;
        console_write("desktop: file browser -> root directory complete\n");
        return;
    }
    console_write("desktop: listing ");
    console_write(entry.directory ? "[DIR] " : "[FILE] ");
    console_write(entry.name);
    console_write(" cluster ");
    console_write_dec(entry.first_cluster);
    console_write(" size ");
    console_write_dec(entry.size);
    console_write("\n");

    if (!entry.directory && entry.size <= sizeof(file_preview) - 1)
    {
        uint32_t preview_size = 0;
        if (fat32_read_root_entry(file_browser_index, file_preview, sizeof(file_preview) - 1,
                                  &preview_size) == 0)
        {
            file_preview[preview_size] = '\0';
            console_write("desktop: preview ");
            for (uint32_t preview_index = 0; preview_index < preview_size; ++preview_index)
            {
                uint8_t character = file_preview[preview_index];
                console_write_char(character >= 32 && character <= 126 ? (char)character : '.');
            }
            console_write("\n");
        }
    }
    file_browser_index += 1;
}

static void desktop_system_loop(void)
{
    console_write("desktop: system monitor -> PCI devices=");
    console_write_dec(pci_device_count());
    console_write(" | scheduler tasks=");
    console_write_dec(scheduler_task_count());
    console_write("\n");
    console_write("desktop: memory pages total=");
    console_write_dec(memory_pages_total());
    console_write(" used=");
    console_write_dec(memory_pages_used());
    console_write(" free=");
    console_write_dec(memory_pages_free());
    console_write("\n");
    console_write("desktop: scheduling policy=");
    console_write(scheduler_performance_mode() ? "PERFORMANCE" : "BALANCED");
    console_write("\n");
    console_write("desktop: gpu=");
    console_write(gpu_acceleration_ready() ? "ACCELERATED" : (gpu_present() ? "DETECTED" : "ABSENT"));
    console_write(" virtio=");
    console_write(gpu_virtio_present() ? "DISCOVERED" : "NO");
    if (gpu_info() != 0 && gpu_info()->virtio_backend)
    {
        console_write(" transport=");
        console_write(gpu_info()->virtio_transport_ready ? "READY" : "INCOMPLETE");
    }
    if (gpu_info() != 0)
    {
        console_write(" vendor=");
        console_write_hex(gpu_info()->vendor_id);
        console_write(" device=");
        console_write_hex(gpu_info()->device_id);
    }
    console_write("\n");
    console_write("desktop: frames=");
    console_write_dec(gpu_frame_count());
    console_write(" rate=");
    console_write_dec(gpu_frame_rate());
    console_write(" fps-window\n");
    console_write("desktop: paging state=");
    console_write(paging_ready() ? "READY" : "OFFLINE");
    console_write(" tables=");
    console_write_dec(paging_table_pages());
    console_write("\n");
    console_write("desktop: syscall entries=");
    console_write_dec(interrupt_syscall_count());
    console_write("\n");
    console_write("desktop: audio input=");
    if (audio_input_state() == AUDIO_INPUT_VOICE)
    {
        console_write("VOICE");
    }
    else if (audio_input_state() == AUDIO_INPUT_READY)
    {
        console_write("READY");
    }
    else
    {
        console_write("OFFLINE");
    }
    console_write(" frames=");
    console_write_dec(audio_frames_received());
    console_write(" events=");
    console_write_dec(audio_voice_events());
    console_write("\n");
    console_write("desktop: mouse=");
    console_write(mouse_present() ? "READY" : "OFFLINE");
    console_write(" x=");
    console_write_dec((uint64_t)mouse_x());
    console_write(" y=");
    console_write_dec((uint64_t)mouse_y());
    console_write(" buttons=");
    console_write_dec(mouse_buttons());
    console_write(" packets=");
    console_write_dec(mouse_packets());
    console_write("\n");
    console_write("desktop: xhci=");
    console_write(xhci_ready() ? "READY" : (xhci_present() ? "PRESENT" : "ABSENT"));
    if (xhci_info() != 0)
    {
        console_write(" ports=");
        console_write_dec(xhci_info()->max_ports);
        console_write(" slots=");
        console_write_dec(xhci_info()->max_slots);
        console_write(" connected=");
        console_write_dec(xhci_connected_ports());
        console_write(" active-port=");
        console_write_dec(xhci_info()->active_port);
        console_write(" context=");
        console_write(xhci_info()->context_ready ? "READY" : "OFFLINE");
        console_write(" address=");
        console_write(xhci_info()->address_ready ? "READY" : "OFFLINE");
        console_write(" rings=");
        console_write(xhci_rings_ready() ? "READY" : "OFFLINE");
    }
    console_write("\n");
    console_write("desktop: user process=");
    console_write(user_process_ready() ? "READY" : "UNPREPARED");
    if (user_process_exited())
    {
        console_write(" exit=");
        console_write_dec(user_process_exit_code());
    }
    console_write("\n");

    for (uint64_t task_index = 0; task_index < scheduler_task_count(); ++task_index)
    {
        scheduler_task_info_t task;
        if (scheduler_task_info(task_index, &task) != 0)
        {
            continue;
        }
        console_write("desktop: task ");
        console_write(task.name != 0 ? task.name : "unnamed");
        console_write(" lane=");
        if (task.lane == SCHEDULER_LANE_USER)
        {
            console_write("USER");
        }
        else if (task.lane == SCHEDULER_LANE_AI)
        {
            console_write("AI");
        }
        else
        {
            console_write("SYSTEM");
        }
        console_write(" state=");
        console_write(task.active != 0 ? "ACTIVE" : "PAUSED");
        console_write("\n");
    }
}

void desktop_init(void)
{
    if (initialized)
    {
        return;
    }
    active_index = 0;
    window_count = 0;
    last_key = 0;
    mode_name[0] = '\0';
    tick_count = 0;
    file_browser_index = 0;
    file_preview[0] = '\0';
    runtime_panel_dirty = 0;
    for (uint8_t index = 0; index < DESKTOP_MAX_WINDOWS; ++index)
    {
        windows[index].active = 0;
        windows[index].x = 0;
        windows[index].y = 0;
        windows[index].width = 0;
        windows[index].height = 0;
        windows[index].title = 0;
    }
    initialized = 1;
    desktop_launch_window("ASSISTANT", 10, 12, 18, 12);
    desktop_launch_window("FILES", 32, 20, 18, 12);
    desktop_launch_window("NETWORK", 18, 26, 18, 12);
    desktop_launch_window("SYSTEM", 44, 16, 18, 12);
    set_active_app(active_index);
    desktop_set_mode("HYBRID");
    refresh_window_state();
    console_write("desktop: autonomous window manager ready\n");
}

void desktop_launch_window(const char *title, uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    if (!initialized || title == 0 || window_count >= DESKTOP_MAX_WINDOWS)
    {
        return;
    }
    windows[window_count].title = title;
    windows[window_count].x = x;
    windows[window_count].y = y;
    windows[window_count].width = width;
    windows[window_count].height = height;
    windows[window_count].active = (window_count == active_index) ? 1 : 0;
    ++window_count;
    console_write("desktop: launched window -> ");
    console_write(title);
    console_write("\n");
}

void desktop_focus_next(void)
{
    if (!initialized || window_count == 0)
    {
        return;
    }
    active_index = (active_index + 1) % window_count;
    set_active_app(active_index);
    refresh_window_state();
    desktop_refresh_runtime_panel();
}

void desktop_focus_previous(void)
{
    if (!initialized || window_count == 0)
    {
        return;
    }
    active_index = (active_index + window_count - 1) % window_count;
    set_active_app(active_index);
    refresh_window_state();
    desktop_refresh_runtime_panel();
}

void desktop_handle_key(char character)
{
    if (!initialized)
    {
        desktop_init();
    }

    last_key = character;
}

void desktop_tick(void)
{
    if (!initialized)
    {
        return;
    }
    tick_count += 1;
    gpu_frame_present(scheduler_ticks());

    if ((tick_count % 90) == 0)
    {
        ui_runtime_overlay(desktop_mode_name(), desktop_active_app(), "RUNTIME MONITORING");
        console_write("desktop: live runtime -> mode=");
        console_write(desktop_mode_name());
        console_write(" | app=");
        console_write(desktop_active_app());
        console_write("\n");
    }

    if (runtime_panel_dirty)
    {
        runtime_panel_dirty = 0;
        desktop_run_active_app();
    }

    if ((tick_count % 75) == 0)
    {
        desktop_focus_next();
        desktop_run_active_app();
    }
}

static void desktop_app_runtime_summary(const char *active)
{
    if (strings_equal(active, "FILES"))
    {
        console_write("desktop: file view sync -> user workspace /boot /kernel /user\n");
        return;
    }

    if (strings_equal(active, "SYSTEM"))
    {
        console_write("desktop: system monitor -> hardware health nominal\n");
        return;
    }

    if (strings_equal(active, "NETWORK"))
    {
        console_write("desktop: network monitor -> tunnel and telemetry ready\n");
        return;
    }

    console_write("desktop: assistant runtime -> user and AI lanes balanced\n");
}

const char *desktop_active_app(void)
{
    if (!initialized || window_count == 0)
    {
        return "ASSISTANT";
    }
    return windows[active_index].title;
}

void desktop_run_active_app(void)
{
    if (!initialized)
    {
        return;
    }

    const char *active = desktop_active_app();
    desktop_app_runtime_summary(active);

    if (strings_equal(desktop_mode_name(), "USER"))
    {
        ui_runtime_overlay(desktop_mode_name(), active, "USER CONTROL PRIORITY");
        console_write("desktop: user lane active -> manual control priority\n");
    }
    else if (strings_equal(desktop_mode_name(), "AI"))
    {
        ui_runtime_overlay(desktop_mode_name(), active, "AI ORCHESTRATION");
        console_write("desktop: AI lane active -> autonomous orchestration enabled\n");
    }
    else
    {
        ui_runtime_overlay(desktop_mode_name(), active, "LANES BALANCED");
        console_write("desktop: hybrid lane active -> user and AI tasks balanced\n");
    }

    if (strings_equal(active, "FILES"))
    {
        desktop_file_browser_loop();
        return;
    }

    if (strings_equal(active, "SYSTEM"))
    {
        desktop_system_loop();
        return;
    }

    if (strings_equal(active, "NETWORK"))
    {
        console_write("desktop: network monitor -> route ready, AI broker online\n");
        return;
    }

    console_write("desktop: assistant runtime -> parsing user intent and task queue\n");
}
