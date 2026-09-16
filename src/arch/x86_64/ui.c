#include "ui.h"

#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8
#define MULTIBOOT_FRAMEBUFFER_RGB 1
#define MAX_UI_WIDTH 1920
#define MAX_UI_HEIGHT 1080

typedef struct
{
    uint8_t *address;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t red_position;
    uint8_t green_position;
    uint8_t blue_position;
    uint8_t red_mask;
    uint8_t green_mask;
    uint8_t blue_mask;
    uint8_t bytes_per_pixel;
} framebuffer_t;

static framebuffer_t framebuffer;
static int available;

static uint32_t read32(const uint8_t *bytes, uint32_t offset)
{
    return (uint32_t)bytes[offset] | ((uint32_t)bytes[offset + 1] << 8) |
           ((uint32_t)bytes[offset + 2] << 16) | ((uint32_t)bytes[offset + 3] << 24);
}

static uint64_t read64(const uint8_t *bytes, uint32_t offset)
{
    return (uint64_t)read32(bytes, offset) | ((uint64_t)read32(bytes, offset + 4) << 32);
}

static void put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!available || x >= framebuffer.width || y >= framebuffer.height)
    {
        return;
    }
    uint8_t red = (uint8_t)(color >> 16);
    uint8_t green = (uint8_t)(color >> 8);
    uint8_t blue = (uint8_t)color;
    uint32_t packed = ((red >> (8 - framebuffer.red_mask)) << framebuffer.red_position) |
                      ((green >> (8 - framebuffer.green_mask)) << framebuffer.green_position) |
                      ((blue >> (8 - framebuffer.blue_mask)) << framebuffer.blue_position);
    uint8_t *pixel = framebuffer.address + y * framebuffer.pitch + x * framebuffer.bytes_per_pixel;
    for (uint8_t byte = 0; byte < framebuffer.bytes_per_pixel; ++byte)
    {
        pixel[byte] = (uint8_t)(packed >> (byte * 8));
    }
}

static void fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    if (x >= framebuffer.width || y >= framebuffer.height)
    {
        return;
    }
    uint32_t right = x + width < framebuffer.width ? x + width : framebuffer.width;
    uint32_t bottom = y + height < framebuffer.height ? y + height : framebuffer.height;
    for (uint32_t row = y; row < bottom; ++row)
    {
        for (uint32_t column = x; column < right; ++column)
        {
            put_pixel(column, row, color);
        }
    }
}

static uint8_t glyph_row(char character, uint8_t row)
{
    static const uint8_t digits[10][7] = {
        {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e},
        {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e},
        {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f},
        {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e},
        {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02},
        {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e},
        {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e},
        {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
        {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e},
        {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c},
    };
    static const uint8_t letters[26][7] = {
        {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
        {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e},
        {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e},
        {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e},
        {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f},
        {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10},
        {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f},
        {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
        {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e},
        {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c},
        {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
        {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f},
        {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11},
        {0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11},
        {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
        {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10},
        {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d},
        {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11},
        {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e},
        {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04},
        {0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11},
        {0x11, 0x0a, 0x04, 0x04, 0x04, 0x0a, 0x11},
        {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04},
        {0x1f, 0x02, 0x04, 0x08, 0x10, 0x10, 0x1f},
    };
    if (row >= 7)
        return 0;
    if (character >= '0' && character <= '9')
        return digits[character - '0'][row];
    if (character >= 'A' && character <= 'Z')
        return letters[character - 'A'][row];
    if (character == '-')
        return row == 3 ? 0x1f : 0;
    if (character == ':')
        return row == 2 || row == 5 ? 0x04 : 0;
    return character == ' ' ? 0 : 0x1f;
}

static void draw_text(uint32_t x, uint32_t y, const char *text, uint32_t color, uint32_t scale)
{
    while (*text != '\0')
    {
        char character = *text++;
        for (uint8_t row = 0; row < 7; ++row)
        {
            uint8_t bits = glyph_row(character, row);
            for (uint8_t column = 0; column < 5; ++column)
            {
                if ((bits & (1u << (4 - column))) != 0)
                {
                    fill_rect(x + column * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        x += 6 * scale;
    }
}

void ui_init(uint64_t multiboot_info_address)
{
    available = 0;
    if (multiboot_info_address == 0)
        return;
    const uint8_t *info = (const uint8_t *)(uintptr_t)multiboot_info_address;
    uint32_t total_size = read32(info, 0);
    if (total_size < 16)
        return;
    const uint8_t *cursor = info + 8;
    const uint8_t *limit = info + total_size;
    while (cursor + 8 <= limit)
    {
        uint32_t type = read32(cursor, 0);
        uint32_t size = read32(cursor, 4);
        if (size < 8 || cursor + size > limit)
            break;
        if (type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER && size >= 36 && cursor[29] == MULTIBOOT_FRAMEBUFFER_RGB)
        {
            uint64_t address = read64(cursor, 8);
            uint32_t pitch = read32(cursor, 16);
            uint32_t width = read32(cursor, 20);
            uint32_t height = read32(cursor, 24);
            uint8_t bpp = cursor[28];
            if (address != 0 && width > 0 && width <= MAX_UI_WIDTH && height > 0 && height <= MAX_UI_HEIGHT &&
                bpp >= 24 && bpp <= 32 && pitch >= width * (bpp / 8) &&
                address + (uint64_t)pitch * height > address && address + (uint64_t)pitch * height <= (1ULL << 30))
            {
                framebuffer.address = (uint8_t *)(uintptr_t)address;
                framebuffer.pitch = pitch;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.bytes_per_pixel = bpp / 8;
                framebuffer.red_position = cursor[30];
                framebuffer.red_mask = cursor[31];
                framebuffer.green_position = cursor[32];
                framebuffer.green_mask = cursor[33];
                framebuffer.blue_position = cursor[34];
                framebuffer.blue_mask = cursor[35];
                available = 1;
                break;
            }
        }
        if (type == MULTIBOOT_TAG_TYPE_END)
            break;
        cursor += (size + 7) & ~7u;
    }
    if (!available)
        return;

    fill_rect(0, 0, framebuffer.width, framebuffer.height, 0x0d1321);
    fill_rect(0, 0, framebuffer.width, 46, 0x1f2a3a);
    fill_rect(18, 11, 24, 24, 0x61dafb);
    draw_text(56, 12, "CHUTOS", 0xf6fbff, 3);

    fill_rect(22, 72, 80, framebuffer.height - 96, 0x141d2d);
    fill_rect(30, 86, 64, 60, 0x24324a);
    fill_rect(30, 156, 64, 60, 0x1e2d3f);
    fill_rect(30, 226, 64, 60, 0x1e2d3f);
    fill_rect(30, 296, 64, 60, 0x1e2d3f);
    draw_text(46, 102, "AI", 0x8be9fd, 2);
    draw_text(42, 172, "SYS", 0xb8f2cb, 2);
    draw_text(42, 242, "APP", 0xffd166, 2);
    draw_text(42, 312, "NET", 0xf9a8d0, 2);

    fill_rect(126, 72, framebuffer.width - 160, framebuffer.height - 110, 0x172338);
    fill_rect(146, 88, framebuffer.width - 210, 120, 0x212e45);
    draw_text(168, 108, "AUTONOMOUS DESKTOP", 0x8be9fd, 3);
    draw_text(168, 156, "VOICE-FIRST AI CONTROL", 0xf3f5ff, 2);

    fill_rect(146, 238, framebuffer.width - 210, 170, 0x1d2940);
    draw_text(170, 260, "SYSTEM CORE", 0x8be9fd, 2);
    draw_text(170, 296, "READY", 0x7ee787, 2);
    draw_text(170, 330, "KERNEL HEALTH: STABLE", 0xf5d76a, 2);
    draw_text(170, 364, "DEVICE GRID: ACTIVE", 0x9ad6ff, 2);

    fill_rect(framebuffer.width - 300, 72, 230, 220, 0x1a2335);
    draw_text(framebuffer.width - 270, 96, "STATUS", 0x8be9fd, 2);
    draw_text(framebuffer.width - 270, 132, "AI", 0xe6edf7, 2);
    draw_text(framebuffer.width - 270, 164, "ONLINE", 0x7ee787, 2);
    draw_text(framebuffer.width - 270, 196, "PCI", 0xf5d76a, 2);
    draw_text(framebuffer.width - 270, 228, "ACTIVE", 0x7ee787, 2);

    fill_rect(146, 438, framebuffer.width - 210, 82, 0x121b29);
    draw_text(170, 462, "AUTO MODE: ACTIVE | AUTHORITY: ONLINE | SILENT KERNEL OPS", 0xf3f7ff, 2);

    fill_rect(146, 540, (framebuffer.width - 210) / 3 - 16, 86, 0x203149);
    fill_rect(146 + (framebuffer.width - 210) / 3, 540, (framebuffer.width - 210) / 3 - 16, 86, 0x203149);
    fill_rect(146 + 2 * ((framebuffer.width - 210) / 3), 540, (framebuffer.width - 210) / 3 - 16, 86, 0x203149);
    draw_text(172, 560, "ASSISTANT", 0x8be9fd, 2);
    draw_text(146 + (framebuffer.width - 210) / 3 + 26, 560, "FILES", 0xb8f2cb, 2);
    draw_text(146 + 2 * ((framebuffer.width - 210) / 3) + 24, 560, "NETWORK", 0xffd166, 2);

    fill_rect(0, framebuffer.height - 34, framebuffer.width, 34, 0x1a2335);
    draw_text(18, framebuffer.height - 26, "chutos://desktop | ai-first runtime | silent control | autonomous management", 0x9ad6ff, 2);
}

int ui_available(void)
{
    return available;
}

void ui_status(const char *title, const char *value)
{
    if (available)
    {
        draw_text(64, 170, title, 0xf3f7ff, 2);
        draw_text(64, 196, value, 0x7ee787, 2);
    }
}

void ui_runtime_overlay(const char *mode, const char *app, const char *activity)
{
    if (!available)
    {
        return;
    }

    uint32_t panel_width = framebuffer.width > 420 ? 360 : framebuffer.width > 150 ? framebuffer.width - 150
                                                                                   : framebuffer.width;
    uint32_t panel_x = framebuffer.width > panel_width + 48 ? framebuffer.width - panel_width - 48 : 0;
    uint32_t panel_y = 316;
    fill_rect(panel_x, panel_y, panel_width, 104, 0x101a2b);
    fill_rect(panel_x, panel_y, 5, 104, 0x61dafb);
    draw_text(panel_x + 18, panel_y + 12, "LIVE TASK", 0x8be9fd, 2);
    draw_text(panel_x + 18, panel_y + 38, "MODE", 0xb9c7d9, 1);
    draw_text(panel_x + 72, panel_y + 38, mode != 0 ? mode : "HYBRID", 0x7ee787, 1);
    draw_text(panel_x + 18, panel_y + 58, "APP", 0xb9c7d9, 1);
    draw_text(panel_x + 72, panel_y + 58, app != 0 ? app : "ASSISTANT", 0xffd166, 1);
    draw_text(panel_x + 18, panel_y + 78, activity != 0 ? activity : "READY", 0xf3f7ff, 1);
}

void ui_ai_conversation(const char *user_text, const char *response)
{
    if (!available || framebuffer.height < 700)
    {
        return;
    }

    uint32_t panel_width = framebuffer.width > 420 ? 360 : framebuffer.width > 150 ? framebuffer.width - 150
                                                                                   : framebuffer.width;
    uint32_t panel_x = framebuffer.width > panel_width + 48 ? framebuffer.width - panel_width - 48 : 0;
    uint32_t panel_y = 432;
    fill_rect(panel_x, panel_y, panel_width, 86, 0x172338);
    fill_rect(panel_x, panel_y, 5, 86, 0xffd166);
    draw_text(panel_x + 18, panel_y + 10, "AI COMPANION", 0xffd166, 2);
    draw_text(panel_x + 18, panel_y + 36, user_text != 0 ? user_text : "LISTENING", 0xf3f7ff, 1);
    draw_text(panel_x + 18, panel_y + 58, response != 0 ? response : "READY", 0x7ee787, 1);
}
