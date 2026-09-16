#include "ata.h"
#include "console.h"
#include "port.h"

#define ATA_DATA 0x1f0
#define ATA_SECTOR_COUNT 0x1f2
#define ATA_LBA_LOW 0x1f3
#define ATA_LBA_MID 0x1f4
#define ATA_LBA_HIGH 0x1f5
#define ATA_DRIVE 0x1f6
#define ATA_STATUS 0x1f7
#define ATA_COMMAND 0x1f7
#define ATA_ALT_STATUS 0x3f6
#define ATA_IDENTIFY 0xec
#define ATA_READ_SECTORS 0x20
#define ATA_STATUS_ERROR 0x01
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_BUSY 0x80
#define ATA_WAIT_LIMIT 1000000u

static int primary_master_present;

static void ata_delay(void)
{
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
}

static int wait_for_data(void)
{
    for (uint32_t attempt = 0; attempt < ATA_WAIT_LIMIT; ++attempt)
    {
        uint8_t status = inb(ATA_STATUS);
        if ((status & ATA_STATUS_ERROR) != 0)
        {
            return -1;
        }
        if ((status & (ATA_STATUS_BUSY | ATA_STATUS_DRQ)) == ATA_STATUS_DRQ)
        {
            return 0;
        }
    }
    return -1;
}

void ata_init(void)
{
    primary_master_present = 0;
    outb(ATA_DRIVE, 0xa0);
    ata_delay();
    if (inb(ATA_STATUS) == 0)
    {
        console_write("ata: primary master absent\n");
        return;
    }
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);
    outb(ATA_COMMAND, ATA_IDENTIFY);
    ata_delay();
    if (wait_for_data() != 0)
    {
        console_write("ata: primary master identify failed\n");
        return;
    }
    for (uint16_t word = 0; word < 256; ++word)
    {
        (void)inw(ATA_DATA);
    }
    primary_master_present = 1;
    console_write("ata: primary master ready for LBA28 reads\n");
}

int ata_present(void)
{
    return primary_master_present;
}

int ata_read_sector(uint32_t lba, uint16_t *buffer)
{
    if (!primary_master_present || buffer == 0 || lba > 0x0fffffff)
    {
        return -1;
    }
    outb(ATA_DRIVE, (uint8_t)(0xe0 | ((lba >> 24) & 0x0f)));
    ata_delay();
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_READ_SECTORS);
    if (wait_for_data() != 0)
    {
        return -1;
    }
    for (uint16_t word = 0; word < 256; ++word)
    {
        buffer[word] = inw(ATA_DATA);
    }
    return 0;
}
