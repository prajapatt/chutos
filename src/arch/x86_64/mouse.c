#include "mouse.h"

#include "console.h"
#include "port.h"

#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64
#define PS2_DATA 0x60
#define PS2_ENABLE_AUX 0xa8
#define PS2_READ_CONFIG 0x20
#define PS2_WRITE_CONFIG 0x60
#define PS2_WRITE_AUX 0xd4
#define MOUSE_ENABLE_REPORTING 0xf4
#define MOUSE_WAIT_LIMIT 100000u
#define MOUSE_MIN_COORDINATE 0
#define MOUSE_MAX_COORDINATE 4095

static uint8_t initialized;
static uint8_t present;
static uint8_t packet_index;
static uint8_t packet[3];
static int32_t cursor_x;
static int32_t cursor_y;
static uint8_t button_state;
static uint64_t packet_count;

static int wait_input_clear(void)
{
    for (uint32_t attempt = 0; attempt < MOUSE_WAIT_LIMIT; ++attempt)
    {
        if ((inb(PS2_STATUS) & 2) == 0)
        {
            return 0;
        }
    }
    return -1;
}

static int wait_output_ready(void)
{
    for (uint32_t attempt = 0; attempt < MOUSE_WAIT_LIMIT; ++attempt)
    {
        if ((inb(PS2_STATUS) & 1) != 0)
        {
            return 0;
        }
    }
    return -1;
}

static int send_mouse_command(uint8_t command)
{
    if (wait_input_clear() != 0)
    {
        return -1;
    }
    outb(PS2_COMMAND, PS2_WRITE_AUX);
    if (wait_input_clear() != 0)
    {
        return -1;
    }
    outb(PS2_DATA, command);
    if (wait_output_ready() != 0)
    {
        return -1;
    }
    return inb(PS2_DATA) == 0xfa ? 0 : -1;
}

static int enable_auxiliary_irq(void)
{
    if (wait_input_clear() != 0)
    {
        return -1;
    }
    outb(PS2_COMMAND, PS2_ENABLE_AUX);
    if (wait_input_clear() != 0)
    {
        return -1;
    }
    outb(PS2_COMMAND, PS2_READ_CONFIG);
    if (wait_output_ready() != 0)
    {
        return -1;
    }
    uint8_t configuration = inb(PS2_DATA);
    configuration |= 0x02;
    if (wait_input_clear() != 0)
    {
        return -1;
    }
    outb(PS2_COMMAND, PS2_WRITE_CONFIG);
    if (wait_input_clear() != 0)
    {
        return -1;
    }
    outb(PS2_DATA, configuration);
    return 0;
}

void mouse_init(void)
{
    initialized = 1;
    present = 0;
    packet_index = 0;
    cursor_x = 0;
    cursor_y = 0;
    button_state = 0;
    packet_count = 0;
    if (enable_auxiliary_irq() == 0 && send_mouse_command(MOUSE_ENABLE_REPORTING) == 0)
    {
        present = 1;
        console_write("mouse: PS/2 auxiliary device reporting enabled\n");
    }
    else
    {
        console_write("mouse: PS/2 auxiliary device unavailable\n");
    }
}

void mouse_interrupt(void)
{
    uint8_t value = inb(PS2_DATA);
    if (!initialized || !present)
    {
        return;
    }
    if (packet_index == 0 && (value & 0x08) == 0)
    {
        return;
    }
    packet[packet_index++] = value;
    if (packet_index < 3)
    {
        return;
    }
    packet_index = 0;
    int32_t delta_x = (int8_t)packet[1];
    int32_t delta_y = (int8_t)packet[2];
    cursor_x += delta_x;
    cursor_y -= delta_y;
    if (cursor_x < MOUSE_MIN_COORDINATE)
    {
        cursor_x = MOUSE_MIN_COORDINATE;
    }
    if (cursor_x > MOUSE_MAX_COORDINATE)
    {
        cursor_x = MOUSE_MAX_COORDINATE;
    }
    if (cursor_y < MOUSE_MIN_COORDINATE)
    {
        cursor_y = MOUSE_MIN_COORDINATE;
    }
    if (cursor_y > MOUSE_MAX_COORDINATE)
    {
        cursor_y = MOUSE_MAX_COORDINATE;
    }
    button_state = packet[0] & 0x07;
    ++packet_count;
}

int mouse_present(void)
{
    return present != 0;
}

int32_t mouse_x(void)
{
    return cursor_x;
}

int32_t mouse_y(void)
{
    return cursor_y;
}

uint8_t mouse_buttons(void)
{
    return button_state;
}

uint64_t mouse_packets(void)
{
    return packet_count;
}
