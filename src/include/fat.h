#pragma once

#include <stdint.h>

typedef struct
{
    uint32_t partition_lba;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint32_t sectors_per_fat;
    uint32_t first_data_lba;
    uint32_t root_cluster;
} fat32_volume_t;

typedef struct
{
    char name[13];
    uint8_t directory;
    uint32_t first_cluster;
    uint32_t size;
} fat32_root_entry_t;

int fat32_init(void);
void fat32_list_root(void);
int fat32_root_entry(uint32_t index, fat32_root_entry_t *entry);
int fat32_read_root_entry(uint32_t index, uint8_t *buffer, uint32_t capacity, uint32_t *size);
int fat32_read_root_83(const uint8_t name[11], uint8_t *buffer, uint32_t capacity, uint32_t *size);
int fat32_mounted(void);
const fat32_volume_t *fat32_volume(void);
