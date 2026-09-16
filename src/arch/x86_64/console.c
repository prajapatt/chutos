#include "console.h"
#include "port.h"

#define VGA_COLUMNS 80
#define VGA_ROWS 25
#define COM1 0x3f8

typedef struct
{
    uint8_t character;
    uint8_t color;
} vga_cell_t;

static volatile vga_cell_t *const vga = (vga_cell_t *)0xb8000;
static size_t cursor_row;
static size_t cursor_column;
static uint8_t text_color = 0x07;

static void serial_put(char character)
{
    while ((inb(COM1 + 5) & 0x20) == 0)
    {
    }
    outb(COM1, (uint8_t)character);
}

static void clear_row(size_t row)
{
    for (size_t column = 0; column < VGA_COLUMNS; ++column)
    {
        vga[row * VGA_COLUMNS + column] = (vga_cell_t){' ', text_color};
    }
}

static void newline(void)
{
    cursor_column = 0;
    if (cursor_row + 1 < VGA_ROWS)
    {
        ++cursor_row;
        return;
    }
    for (size_t row = 1; row < VGA_ROWS; ++row)
    {
        for (size_t column = 0; column < VGA_COLUMNS; ++column)
        {
            vga[(row - 1) * VGA_COLUMNS + column] = vga[row * VGA_COLUMNS + column];
        }
    }
    clear_row(VGA_ROWS - 1);
}

void console_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xc7);
    outb(COM1 + 4, 0x0b);
    cursor_row = 0;
    cursor_column = 0;
    console_clear();
}

void console_clear(void)
{
    for (size_t row = 0; row < VGA_ROWS; ++row)
    {
        clear_row(row);
    }
    cursor_row = 0;
    cursor_column = 0;
}

void console_write_char(char character)
{
    serial_put(character);
    if (character == '\n')
    {
        newline();
        return;
    }
    vga[cursor_row * VGA_COLUMNS + cursor_column] = (vga_cell_t){(uint8_t)character, text_color};
    if (++cursor_column == VGA_COLUMNS)
    {
        newline();
    }
}

void console_write(const char *text)
{
    while (*text != '\0')
    {
        console_write_char(*text++);
    }
}

void console_write_hex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    console_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
    {
        char digit[2] = {digits[(value >> shift) & 0xf], '\0'};
        console_write(digit);
    }
}

void console_write_dec(uint64_t value)
{
    char digits[21];
    size_t length = 0;
    if (value == 0)
    {
        console_write("0");
        return;
    }
    while (value != 0)
    {
        digits[length++] = (char)('0' + value % 10);
        value /= 10;
    }
    while (length != 0)
    {
        char digit[2] = {digits[--length], '\0'};
        console_write(digit);
    }
}
