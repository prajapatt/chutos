#include "block.h"
#include "ata.h"

static uint16_t sector_words[256];

int block_read(uint32_t lba, void *buffer)
{
    if (buffer == 0 || ata_read_sector(lba, sector_words) != 0)
    {
        return -1;
    }
    uint8_t *bytes = (uint8_t *)buffer;
    for (uint16_t word = 0; word < 256; ++word)
    {
        bytes[word * 2] = (uint8_t)sector_words[word];
        bytes[word * 2 + 1] = (uint8_t)(sector_words[word] >> 8);
    }
    return 0;
}
