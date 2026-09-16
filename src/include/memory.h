#pragma once

#include <stdint.h>

void memory_init(uint64_t multiboot_info_address);
void *memory_alloc_page(void);
uint64_t memory_pages_total(void);
uint64_t memory_pages_used(void);
uint64_t memory_pages_free(void);
