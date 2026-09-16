#include "memory.h"

#define PAGE_SIZE 4096
#define MAX_MEMORY_REGIONS 32
#define IDENTITY_MAPPED_LIMIT (1ULL << 30)

typedef struct
{
    uint64_t start;
    uint64_t end;
} memory_region_t;

typedef struct __attribute__((packed))
{
    uint32_t type;
    uint32_t size;
    uint64_t address;
    uint64_t length;
    uint32_t kind;
} multiboot_memory_entry_t;

extern uint8_t __kernel_end;
static memory_region_t regions[MAX_MEMORY_REGIONS];
static uint32_t region_count;
static uint32_t current_region;
static uintptr_t next_page;
static uint64_t pages_total;
static uint64_t pages_used;

static uint64_t align_up(uint64_t address)
{
    return (address + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static void add_region(uint64_t start, uint64_t end)
{
    if (start >= IDENTITY_MAPPED_LIMIT)
    {
        return;
    }
    if (end > IDENTITY_MAPPED_LIMIT)
    {
        end = IDENTITY_MAPPED_LIMIT;
    }
    if (region_count == MAX_MEMORY_REGIONS || end <= start)
    {
        return;
    }
    uint64_t aligned_start = align_up(start);
    uint64_t aligned_end = end & ~(PAGE_SIZE - 1);
    if (aligned_end <= aligned_start)
    {
        return;
    }
    regions[region_count++] = (memory_region_t){aligned_start, aligned_end};
    pages_total += (aligned_end - aligned_start) / PAGE_SIZE;
}

void memory_init(uint64_t multiboot_info_address)
{
    const uint64_t kernel_end = align_up((uintptr_t)&__kernel_end);
    region_count = 0;
    current_region = 0;
    next_page = kernel_end;
    pages_total = 0;
    pages_used = 0;

    if (multiboot_info_address != 0)
    {
        const uint32_t total_size = *(const uint32_t *)(uintptr_t)multiboot_info_address;
        const uint8_t *cursor = (const uint8_t *)(uintptr_t)multiboot_info_address + 8;
        const uint8_t *limit = (const uint8_t *)(uintptr_t)multiboot_info_address + total_size;
        while (cursor + 8 <= limit)
        {
            const uint32_t type = *(const uint32_t *)cursor;
            const uint32_t size = *(const uint32_t *)(cursor + 4);
            if (size < 8 || cursor + size > limit)
            {
                break;
            }
            if (type == 6 && size >= 16)
            {
                const uint8_t *entry_cursor = cursor + 16;
                const uint8_t *entry_limit = cursor + size;
                const uint32_t entry_size = *(const uint32_t *)(cursor + 8);
                while (entry_size >= 24 && entry_cursor + entry_size <= entry_limit)
                {
                    const multiboot_memory_entry_t *entry = (const multiboot_memory_entry_t *)entry_cursor;
                    if (entry->kind == 1 && entry->length != 0)
                    {
                        uint64_t end = entry->address + entry->length;
                        if (end > entry->address)
                        {
                            uint64_t start = entry->address < kernel_end ? kernel_end : entry->address;
                            add_region(start, end);
                        }
                    }
                    entry_cursor += entry_size;
                }
            }
            cursor += (size + 7) & ~7u;
        }
    }

    if (region_count == 0)
    {
        add_region(kernel_end, 128 * 1024 * 1024);
    }
}

void *memory_alloc_page(void)
{
    while (current_region < region_count)
    {
        memory_region_t *region = &regions[current_region];
        if (next_page < region->start)
        {
            next_page = region->start;
        }
        if (next_page + PAGE_SIZE <= next_page || next_page + PAGE_SIZE > region->end)
        {
            ++current_region;
            if (current_region < region_count)
            {
                next_page = regions[current_region].start;
            }
            continue;
        }
        void *page = (void *)next_page;
        next_page += PAGE_SIZE;
        ++pages_used;
        uint8_t *bytes = (uint8_t *)page;
        for (uint32_t offset = 0; offset < PAGE_SIZE; ++offset)
        {
            bytes[offset] = 0;
        }
        return page;
    }
    return 0;
}

uint64_t memory_pages_total(void)
{
    return pages_total;
}

uint64_t memory_pages_used(void)
{
    return pages_used;
}

uint64_t memory_pages_free(void)
{
    return pages_used < pages_total ? pages_total - pages_used : 0;
}
