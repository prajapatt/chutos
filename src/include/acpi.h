#pragma once

#include <stdint.h>

typedef struct
{
    uint8_t id;
    uint32_t address;
    uint32_t gsi_base;
} acpi_ioapic_t;

void acpi_init(void);
int acpi_available(void);
uintptr_t acpi_rsdp_address(void);
int acpi_madt_available(void);
uint32_t acpi_local_apic_address(void);
uint32_t acpi_ioapic_count(void);
const acpi_ioapic_t *acpi_ioapic(uint32_t index);
