#pragma once

#include <stdint.h>

void ata_init(void);
int ata_present(void);
int ata_read_sector(uint32_t lba, uint16_t *buffer);
