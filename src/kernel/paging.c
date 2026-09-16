#include "paging.h"

#include "console.h"
#include "memory.h"

#define PAGE_SIZE 4096ULL
#define PAGE_TABLE_ENTRIES 512
#define PAGE_TABLE_PRESENT 0x001ULL
#define PAGE_TABLE_WRITABLE 0x002ULL
#define PAGE_TABLE_USER 0x004ULL
#define PAGE_TABLE_LARGE 0x080ULL
#define IDENTITY_MAPPED_BYTES (1ULL << 30)
#define LARGE_PAGE_SIZE (2ULL << 20)

static uint8_t ready;
static uint64_t table_pages;
static uint64_t *runtime_pdpt;
static uint64_t *user_directory;
static uint64_t *user_table;

struct paging_address_space
{
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *user_directory;
    uint64_t *user_table;
};

static paging_address_space_t user_address_space;
static uint8_t user_address_space_ready;

static uint64_t page_address(void *page)
{
    return (uint64_t)(uintptr_t)page;
}

static void load_page_table(uint64_t address)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(address) : "memory");
}

void paging_init(void)
{
    ready = 0;
    table_pages = 0;
    runtime_pdpt = 0;
    user_directory = 0;
    user_table = 0;
    user_address_space_ready = 0;

    uint64_t *pml4 = (uint64_t *)memory_alloc_page();
    uint64_t *pdpt = (uint64_t *)memory_alloc_page();
    if (pml4 == 0 || pdpt == 0)
    {
        console_write("paging: unable to allocate root tables\n");
        return;
    }
    table_pages = 2;
    runtime_pdpt = pdpt;
    pml4[0] = page_address(pdpt) | PAGE_TABLE_PRESENT | PAGE_TABLE_WRITABLE;

    const uint64_t page_directory_count = IDENTITY_MAPPED_BYTES / (PAGE_TABLE_ENTRIES * LARGE_PAGE_SIZE);
    for (uint64_t directory_index = 0; directory_index < page_directory_count; ++directory_index)
    {
        uint64_t *directory = (uint64_t *)memory_alloc_page();
        if (directory == 0)
        {
            console_write("paging: page-directory allocation failed\n");
            return;
        }
        ++table_pages;
        pdpt[directory_index] = page_address(directory) | PAGE_TABLE_PRESENT | PAGE_TABLE_WRITABLE;
        for (uint64_t entry_index = 0; entry_index < PAGE_TABLE_ENTRIES; ++entry_index)
        {
            uint64_t physical_address = (directory_index * PAGE_TABLE_ENTRIES + entry_index) * LARGE_PAGE_SIZE;
            directory[entry_index] = physical_address | PAGE_TABLE_PRESENT | PAGE_TABLE_WRITABLE |
                                     PAGE_TABLE_LARGE;
        }
    }

    load_page_table(page_address(pml4));
    ready = 1;
    console_write("paging: runtime identity map installed, tables=");
    console_write_dec(table_pages);
    console_write("\n");
}

int paging_ready(void)
{
    return ready != 0;
}

uint64_t paging_table_pages(void)
{
    return table_pages;
}

static void invalidate_page(uint64_t virtual_address)
{
    __asm__ volatile("invlpg (%0)" : : "r"((void *)(uintptr_t)virtual_address) : "memory");
}

int paging_map_user_page(uint64_t virtual_address, uint64_t physical_address)
{
    if (!ready || (virtual_address & (PAGE_SIZE - 1)) != 0 ||
        (physical_address & (PAGE_SIZE - 1)) != 0 || virtual_address < PAGING_USER_BASE ||
        virtual_address >= PAGING_USER_BASE + PAGING_USER_WINDOW_SIZE)
    {
        return -1;
    }

    if (user_directory == 0)
    {
        user_directory = (uint64_t *)memory_alloc_page();
        if (user_directory == 0)
        {
            return -1;
        }
        ++table_pages;
        runtime_pdpt[1] = page_address(user_directory) | PAGE_TABLE_PRESENT | PAGE_TABLE_WRITABLE |
                          PAGE_TABLE_USER;
    }

    if (user_table == 0)
    {
        user_table = (uint64_t *)memory_alloc_page();
        if (user_table == 0)
        {
            return -1;
        }
        ++table_pages;
        user_directory[0] = page_address(user_table) | PAGE_TABLE_PRESENT | PAGE_TABLE_WRITABLE |
                            PAGE_TABLE_USER;
    }

    uint64_t page_index = (virtual_address - PAGING_USER_BASE) / PAGE_SIZE;
    user_table[page_index] = physical_address | PAGE_TABLE_PRESENT | PAGE_TABLE_WRITABLE | PAGE_TABLE_USER;
    invalidate_page(virtual_address);
    return 0;
}

paging_address_space_t *paging_create_address_space(void)
{
    if (!ready || user_address_space_ready)
    {
        return user_address_space_ready != 0 ? &user_address_space : 0;
    }

    uint64_t *pml4 = (uint64_t *)memory_alloc_page();
    uint64_t *pdpt = (uint64_t *)memory_alloc_page();
    if (pml4 == 0 || pdpt == 0)
    {
        return 0;
    }

    pml4[0] = page_address(pdpt) | PAGE_TABLE_PRESENT | PAGE_TABLE_WRITABLE;
    pdpt[0] = runtime_pdpt[0];
    user_address_space.pml4 = pml4;
    user_address_space.pdpt = pdpt;
    user_address_space.user_directory = 0;
    user_address_space.user_table = 0;
    user_address_space_ready = 1;
    table_pages += 2;
    return &user_address_space;
}

int paging_map_address_space_page(paging_address_space_t *address_space, uint64_t virtual_address,
                                  uint64_t physical_address)
{
    if (address_space == 0 || address_space != &user_address_space || !ready ||
        (virtual_address & (PAGE_SIZE - 1)) != 0 || (physical_address & (PAGE_SIZE - 1)) != 0 ||
        virtual_address < PAGING_USER_BASE ||
        virtual_address >= PAGING_USER_BASE + PAGING_USER_WINDOW_SIZE)
    {
        return -1;
    }

    if (address_space->user_directory == 0)
    {
        address_space->user_directory = (uint64_t *)memory_alloc_page();
        if (address_space->user_directory == 0)
        {
            return -1;
        }
        ++table_pages;
        address_space->pdpt[1] = page_address(address_space->user_directory) |
                                 PAGE_TABLE_PRESENT | PAGE_TABLE_WRITABLE | PAGE_TABLE_USER;
    }

    if (address_space->user_table == 0)
    {
        address_space->user_table = (uint64_t *)memory_alloc_page();
        if (address_space->user_table == 0)
        {
            return -1;
        }
        ++table_pages;
        address_space->user_directory[0] = page_address(address_space->user_table) |
                                           PAGE_TABLE_PRESENT | PAGE_TABLE_WRITABLE | PAGE_TABLE_USER;
    }

    uint64_t page_index = (virtual_address - PAGING_USER_BASE) / PAGE_SIZE;
    address_space->user_table[page_index] = physical_address | PAGE_TABLE_PRESENT |
                                            PAGE_TABLE_WRITABLE | PAGE_TABLE_USER;
    return 0;
}

int paging_switch_address_space(paging_address_space_t *address_space)
{
    if (address_space == 0 || address_space->pml4 == 0 || !ready)
    {
        return -1;
    }
    load_page_table(page_address(address_space->pml4));
    return 0;
}

void *paging_alloc_user_page(void)
{
    void *physical_page = memory_alloc_page();
    if (physical_page == 0)
    {
        return 0;
    }

    static uint64_t next_user_page = PAGING_USER_BASE;
    uint64_t virtual_address = next_user_page;
    if (paging_map_user_page(virtual_address, page_address(physical_page)) != 0)
    {
        return 0;
    }
    next_user_page += PAGE_SIZE;
    return (void *)(uintptr_t)virtual_address;
}
