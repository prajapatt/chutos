#include "keyboard.h"
#include "port.h"

#define KEYBOARD_DATA 0x60
#define KEYBOARD_QUEUE_SIZE 256

static const char normal_map[128] = {
    [2] = '1', [3] = '2', [4] = '3', [5] = '4', [6] = '5', [7] = '6', [8] = '7', [9] = '8', [10] = '9', [11] = '0', [12] = '-', [13] = '=', [14] = '\b', [15] = '\t', [16] = 'q', [17] = 'w', [18] = 'e', [19] = 'r', [20] = 't', [21] = 'y', [22] = 'u', [23] = 'i', [24] = 'o', [25] = 'p', [26] = '[', [27] = ']', [28] = '\n', [30] = 'a', [31] = 's', [32] = 'd', [33] = 'f', [34] = 'g', [35] = 'h', [36] = 'j', [37] = 'k', [38] = 'l', [39] = ';', [40] = '\'', [41] = '`', [43] = '\\', [44] = 'z', [45] = 'x', [46] = 'c', [47] = 'v', [48] = 'b', [49] = 'n', [50] = 'm', [51] = ',', [52] = '.', [53] = '/', [57] = ' '};

static const char shifted_map[128] = {
    [2] = '!', [3] = '@', [4] = '#', [5] = '$', [6] = '%', [7] = '^', [8] = '&', [9] = '*', [10] = '(', [11] = ')', [12] = '_', [13] = '+', [14] = '\b', [15] = '\t', [16] = 'Q', [17] = 'W', [18] = 'E', [19] = 'R', [20] = 'T', [21] = 'Y', [22] = 'U', [23] = 'I', [24] = 'O', [25] = 'P', [26] = '{', [27] = '}', [28] = '\n', [30] = 'A', [31] = 'S', [32] = 'D', [33] = 'F', [34] = 'G', [35] = 'H', [36] = 'J', [37] = 'K', [38] = 'L', [39] = ':', [40] = '"', [41] = '~', [43] = '|', [44] = 'Z', [45] = 'X', [46] = 'C', [47] = 'V', [48] = 'B', [49] = 'N', [50] = 'M', [51] = '<', [52] = '>', [53] = '?', [57] = ' '};

static char queue[KEYBOARD_QUEUE_SIZE];
static uint8_t queue_read;
static uint8_t queue_write;
static uint8_t shift_down;

void keyboard_interrupt(void)
{
    uint8_t scancode = inb(KEYBOARD_DATA);
    if (scancode == 0x2a || scancode == 0x36)
    {
        shift_down = 1;
        return;
    }
    if (scancode == 0xaa || scancode == 0xb6)
    {
        shift_down = 0;
        return;
    }
    if ((scancode & 0x80) != 0 || scancode >= 128)
    {
        return;
    }
    char character = (shift_down ? shifted_map : normal_map)[scancode];
    if (character == '\0')
    {
        return;
    }
    uint8_t next = (uint8_t)(queue_write + 1);
    if (next == queue_read)
    {
        return;
    }
    queue[queue_write] = character;
    queue_write = next;
}

int keyboard_read(char *character)
{
    if (character == 0 || queue_read == queue_write)
    {
        return 0;
    }
    *character = queue[queue_read];
    queue_read = (uint8_t)(queue_read + 1);
    return 1;
}
