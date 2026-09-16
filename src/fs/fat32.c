#include "fat.h"
#include "ata.h"
#include "block.h"
#include "console.h"

static uint8_t sector[512];
static fat32_volume_t mounted_volume;
static int mounted;

static uint16_t read16(const uint8_t *bytes, uint32_t offset)
{
    return (uint16_t)bytes[offset] | ((uint16_t)bytes[offset + 1] << 8);
}

static uint32_t read32(const uint8_t *bytes, uint32_t offset)
{
    return (uint32_t)bytes[offset] | ((uint32_t)bytes[offset + 1] << 8) |
           ((uint32_t)bytes[offset + 2] << 16) | ((uint32_t)bytes[offset + 3] << 24);
}

static int valid_boot_sector(const uint8_t *bytes)
{
    uint16_t bytes_per_sector = read16(bytes, 11);
    uint8_t sectors_per_cluster = bytes[13];
    uint16_t reserved_sectors = read16(bytes, 14);
    uint8_t fat_count = bytes[16];
    uint32_t sectors_per_fat = read32(bytes, 36);
    uint32_t root_cluster = read32(bytes, 44);
    return bytes[510] == 0x55 && bytes[511] == 0xaa && bytes_per_sector == 512 &&
           sectors_per_cluster != 0 && reserved_sectors != 0 && fat_count != 0 &&
           sectors_per_fat != 0 && root_cluster >= 2;
}

static int load_volume(uint32_t partition_lba)
{
    if (block_read(partition_lba, sector) != 0 || !valid_boot_sector(sector))
    {
        return -1;
    }
    mounted_volume.partition_lba = partition_lba;
    mounted_volume.bytes_per_sector = read16(sector, 11);
    mounted_volume.sectors_per_cluster = sector[13];
    mounted_volume.reserved_sectors = read16(sector, 14);
    mounted_volume.fat_count = sector[16];
    mounted_volume.sectors_per_fat = read32(sector, 36);
    mounted_volume.first_data_lba = partition_lba + mounted_volume.reserved_sectors +
                                    mounted_volume.fat_count * mounted_volume.sectors_per_fat;
    mounted_volume.root_cluster = read32(sector, 44);
    mounted = 1;
    return 0;
}

static uint32_t next_cluster(uint32_t cluster)
{
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = mounted_volume.partition_lba + mounted_volume.reserved_sectors +
                          fat_offset / mounted_volume.bytes_per_sector;
    if (cluster > 0x0fffffffu / 4 || block_read(fat_sector, sector) != 0)
    {
        return 0x0ffffff7;
    }
    return read32(sector, fat_offset % mounted_volume.bytes_per_sector) & 0x0fffffffu;
}

static uint32_t cluster_lba(uint32_t cluster, uint16_t sector_offset)
{
    return mounted_volume.first_data_lba +
           (cluster - 2) * mounted_volume.sectors_per_cluster + sector_offset;
}

static int read_file_clusters(uint32_t cluster, uint32_t file_size, uint8_t *buffer, uint32_t capacity,
                              uint32_t *size)
{
    if (file_size > capacity)
    {
        return -1;
    }

    uint32_t bytes_read = 0;
    for (uint16_t cluster_steps = 0; bytes_read < file_size && cluster_steps < 1024 &&
                                     cluster >= 2 && cluster < 0x0ffffff8u;
         ++cluster_steps)
    {
        for (uint16_t sector_offset = 0; sector_offset < mounted_volume.sectors_per_cluster &&
                                         bytes_read < file_size;
             ++sector_offset)
        {
            if (block_read(cluster_lba(cluster, sector_offset), sector) != 0)
            {
                return -1;
            }
            uint32_t remaining = file_size - bytes_read;
            uint32_t amount = remaining < sizeof(sector) ? remaining : sizeof(sector);
            for (uint32_t index = 0; index < amount; ++index)
            {
                buffer[bytes_read + index] = sector[index];
            }
            bytes_read += amount;
        }
        cluster = next_cluster(cluster);
    }

    if (bytes_read != file_size)
    {
        return -1;
    }
    *size = bytes_read;
    return 0;
}

static void print_short_name(const uint8_t *entry)
{
    uint8_t name_length = 8;
    uint8_t extension_length = 3;
    while (name_length != 0 && entry[name_length - 1] == ' ')
    {
        --name_length;
    }
    while (extension_length != 0 && entry[8 + extension_length - 1] == ' ')
    {
        --extension_length;
    }
    for (uint8_t index = 0; index < name_length; ++index)
    {
        console_write_char((char)entry[index]);
    }
    if (extension_length != 0)
    {
        console_write_char('.');
        for (uint8_t index = 0; index < extension_length; ++index)
        {
            console_write_char((char)entry[8 + index]);
        }
    }
}

static void format_short_name(const uint8_t *entry, char name[13])
{
    uint8_t name_length = 8;
    uint8_t extension_length = 3;
    uint8_t output = 0;
    while (name_length != 0 && entry[name_length - 1] == ' ')
    {
        --name_length;
    }
    while (extension_length != 0 && entry[8 + extension_length - 1] == ' ')
    {
        --extension_length;
    }
    for (uint8_t index = 0; index < name_length && output < 12; ++index)
    {
        name[output++] = (char)entry[index];
    }
    if (extension_length != 0 && output < 12)
    {
        name[output++] = '.';
        for (uint8_t index = 0; index < extension_length && output < 12; ++index)
        {
            name[output++] = (char)entry[8 + index];
        }
    }
    name[output] = '\0';
}

static int find_root_file(const uint8_t name[11], uint32_t *first_cluster, uint32_t *file_size)
{
    uint32_t cluster = mounted_volume.root_cluster;
    for (uint16_t cluster_steps = 0; cluster_steps < 1024 && cluster >= 2 && cluster < 0x0ffffff8u;
         ++cluster_steps)
    {
        for (uint16_t sector_offset = 0; sector_offset < mounted_volume.sectors_per_cluster; ++sector_offset)
        {
            if (block_read(cluster_lba(cluster, sector_offset), sector) != 0)
            {
                return -1;
            }
            for (uint16_t entry_offset = 0; entry_offset < 512; entry_offset += 32)
            {
                const uint8_t *entry = &sector[entry_offset];
                if (entry[0] == 0x00)
                {
                    return -1;
                }
                if (entry[0] == 0xe5 || entry[11] == 0x0f || (entry[11] & 0x18) != 0)
                {
                    continue;
                }
                uint8_t matching_bytes = 0;
                for (uint8_t index = 0; index < 11; ++index)
                {
                    if (entry[index] == name[index])
                    {
                        ++matching_bytes;
                    }
                }
                if (matching_bytes == 11)
                {
                    *first_cluster = ((uint32_t)read16(entry, 20) << 16) | read16(entry, 26);
                    *file_size = read32(entry, 28);
                    return 0;
                }
            }
        }
        cluster = next_cluster(cluster);
    }
    return -1;
}

int fat32_init(void)
{
    mounted = 0;
    if (!ata_present())
    {
        console_write("fat32: no ATA volume\n");
        return -1;
    }
    if (load_volume(0) != 0)
    {
        if (block_read(0, sector) != 0 || sector[510] != 0x55 || sector[511] != 0xaa)
        {
            console_write("fat32: no boot sector or MBR\n");
            return -1;
        }
        for (uint8_t partition = 0; partition < 4; ++partition)
        {
            uint32_t entry = 446 + partition * 16;
            uint8_t type = sector[entry + 4];
            if (type == 0x0b || type == 0x0c)
            {
                uint32_t partition_lba = read32(sector, entry + 8);
                if (load_volume(partition_lba) == 0)
                {
                    break;
                }
            }
        }
    }
    if (!mounted)
    {
        console_write("fat32: no valid FAT32 volume\n");
        return -1;
    }
    console_write("fat32: mounted root cluster ");
    console_write_dec(mounted_volume.root_cluster);
    console_write(" data LBA ");
    console_write_dec(mounted_volume.first_data_lba);
    console_write("\n");
    return 0;
}

void fat32_list_root(void)
{
    if (!mounted)
    {
        return;
    }
    console_write("fat32: root directory\n");
    uint32_t cluster = mounted_volume.root_cluster;
    for (uint16_t cluster_steps = 0; cluster_steps < 1024 && cluster >= 2 && cluster < 0x0ffffff8u;
         ++cluster_steps)
    {
        for (uint16_t sector_offset = 0; sector_offset < mounted_volume.sectors_per_cluster; ++sector_offset)
        {
            if (block_read(cluster_lba(cluster, sector_offset), sector) != 0)
            {
                console_write("fat32: directory read failed\n");
                return;
            }
            for (uint16_t entry_offset = 0; entry_offset < 512; entry_offset += 32)
            {
                const uint8_t *entry = &sector[entry_offset];
                if (entry[0] == 0x00)
                {
                    return;
                }
                if (entry[0] == 0xe5 || entry[11] == 0x0f || entry[11] == 0x08)
                {
                    continue;
                }
                uint32_t entry_cluster = ((uint32_t)read16(entry, 20) << 16) | read16(entry, 26);
                console_write((entry[11] & 0x10) != 0 ? "[DIR] " : "[FILE] ");
                print_short_name(entry);
                console_write(" cluster ");
                console_write_dec(entry_cluster);
                console_write(" size ");
                console_write_dec(read32(entry, 28));
                console_write("\n");
            }
        }
        cluster = next_cluster(cluster);
    }
}

int fat32_root_entry(uint32_t index, fat32_root_entry_t *result)
{
    if (!mounted || result == 0)
    {
        return -1;
    }

    uint32_t visible_index = 0;
    uint32_t cluster = mounted_volume.root_cluster;
    for (uint16_t cluster_steps = 0; cluster_steps < 1024 && cluster >= 2 && cluster < 0x0ffffff8u;
         ++cluster_steps)
    {
        for (uint16_t sector_offset = 0; sector_offset < mounted_volume.sectors_per_cluster; ++sector_offset)
        {
            if (block_read(cluster_lba(cluster, sector_offset), sector) != 0)
            {
                return -1;
            }
            for (uint16_t entry_offset = 0; entry_offset < 512; entry_offset += 32)
            {
                const uint8_t *directory_entry = &sector[entry_offset];
                if (directory_entry[0] == 0x00)
                {
                    return -1;
                }
                if (directory_entry[0] == 0xe5 || directory_entry[11] == 0x0f ||
                    directory_entry[11] == 0x08)
                {
                    continue;
                }
                if (visible_index++ != index)
                {
                    continue;
                }
                format_short_name(directory_entry, result->name);
                result->directory = (directory_entry[11] & 0x10) != 0;
                result->first_cluster = ((uint32_t)read16(directory_entry, 20) << 16) |
                                        read16(directory_entry, 26);
                result->size = read32(directory_entry, 28);
                return 0;
            }
        }
        cluster = next_cluster(cluster);
    }
    return -1;
}

int fat32_read_root_entry(uint32_t index, uint8_t *buffer, uint32_t capacity, uint32_t *size)
{
    if (!mounted || buffer == 0 || size == 0)
    {
        return -1;
    }

    fat32_root_entry_t entry;
    if (fat32_root_entry(index, &entry) != 0 || entry.directory != 0)
    {
        return -1;
    }
    return read_file_clusters(entry.first_cluster, entry.size, buffer, capacity, size);
}

int fat32_read_root_83(const uint8_t name[11], uint8_t *buffer, uint32_t capacity, uint32_t *size)
{
    if (!mounted || name == 0 || buffer == 0 || size == 0)
    {
        return -1;
    }
    uint32_t cluster;
    uint32_t file_size;
    if (find_root_file(name, &cluster, &file_size) != 0 || file_size > capacity)
    {
        return -1;
    }
    return read_file_clusters(cluster, file_size, buffer, capacity, size);
}

int fat32_mounted(void)
{
    return mounted;
}

const fat32_volume_t *fat32_volume(void)
{
    return mounted ? &mounted_volume : 0;
}
