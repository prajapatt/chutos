#include "console.h"
#include "acpi.h"
#include "ai.h"
#include "audio.h"
#include "ata.h"
#include "cpu.h"
#include "desktop.h"
#include "fat.h"
#include "gpu.h"
#include "keyboard.h"
#include "interrupts.h"
#include "memory.h"
#include "mouse.h"
#include "paging.h"
#include "pci.h"
#include "scheduler.h"
#include "ui.h"
#include "user_process.h"
#include "xhci.h"

static void heartbeat_task(void)
{
    static uint64_t last_report;
    uint64_t now = scheduler_ticks();
    if (now - last_report < 100)
    {
        return;
    }
    last_report = now;
    console_write("[chutos] scheduler tick ");
    console_write_dec(now);
    console_write("\n");
}

static void input_task(void)
{
    char character;
    if (keyboard_read(&character))
    {
        ai_feed_key(character);
        desktop_handle_key(character);
        console_write_char(character);
    }
}

static void desktop_task(void)
{
    desktop_tick();
}

static void audio_runtime_task(void)
{
    audio_task();
}

void kernel_main(uint64_t multiboot_info_address)
{
    console_init();
    console_write("Chutos x86_64 kernel\n");
    console_write("boot: multiboot2, long mode, serial COM1\n");
    cpu_init();
    audio_init();
    mouse_init();
    ui_init(multiboot_info_address);
    if (ui_available())
    {
        ui_status("BOOT", "CHUTOS ONLINE");
    }
    memory_init(multiboot_info_address);
    console_write("memory: Multiboot2 usable-page allocator online\n");
    paging_init();
    paging_address_space_t *kernel_address_space = paging_create_address_space();
    if (kernel_address_space != 0 && paging_switch_address_space(kernel_address_space) == 0)
    {
        console_write("paging: isolated kernel address-space root active\n");
    }
    else
    {
        console_write("paging: isolated address-space root unavailable\n");
    }
    user_process_prepare();
    pci_init();
    gpu_init();
    xhci_init();
    acpi_init();
    ata_init();
    ai_init();
    desktop_init();
    if (fat32_init() == 0)
    {
        fat32_list_root();
    }
    interrupts_init();
    if (scheduler_add_lane("heartbeat", heartbeat_task, SCHEDULER_LANE_SYSTEM) != 0)
    {
        console_write("scheduler: task table full\n");
        return;
    }
    if (scheduler_add_lane("input", input_task, SCHEDULER_LANE_USER) != 0)
    {
        console_write("scheduler: input task unavailable\n");
        return;
    }
    if (scheduler_add_lane("ai", ai_task, SCHEDULER_LANE_AI) != 0)
    {
        console_write("scheduler: ai task unavailable\n");
        return;
    }
    if (scheduler_add_lane("desktop", desktop_task, SCHEDULER_LANE_AI) != 0)
    {
        console_write("scheduler: desktop task unavailable\n");
        return;
    }
    if (scheduler_add_lane("audio", audio_runtime_task, SCHEDULER_LANE_USER) != 0)
    {
        console_write("scheduler: audio task unavailable\n");
        return;
    }
    console_write("kernel: interrupts and agenda online\n");
    console_write("AI: autonomous voice-first runtime engaged\n");
    console_write("AI: authority layer engaged\n");
    if (ui_available())
    {
        ui_status("AI CORE", "AI SYSTEM ACTIVE");
    }
    interrupts_enable();
    for (;;)
    {
        scheduler_run_once();
        __asm__ volatile("hlt");
    }
}
