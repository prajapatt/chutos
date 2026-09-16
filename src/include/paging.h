#pragma once

#include <stdint.h>

#define PAGING_USER_BASE 0x40000000ULL
#define PAGING_USER_WINDOW_SIZE 0x200000ULL

typedef struct paging_address_space paging_address_space_t;

void paging_init(void);
int paging_ready(void);
int paging_map_user_page(uint64_t virtual_address, uint64_t physical_address);
void *paging_alloc_user_page(void);
paging_address_space_t *paging_create_address_space(void);
int paging_map_address_space_page(paging_address_space_t *address_space, uint64_t virtual_address,
                                  uint64_t physical_address);
int paging_switch_address_space(paging_address_space_t *address_space);
uint64_t paging_table_pages(void);
