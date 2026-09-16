#pragma once

#include <stdint.h>

#define PCI_MAX_CAPABILITIES 16

typedef struct
{
    uint8_t id;
    uint8_t offset;
    uint8_t type;
    uint8_t bar;
    uint32_t bar_offset;
    uint32_t length;
} pci_capability_t;

typedef struct
{
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t header_type;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t programming_interface;
    uint32_t bars[6];
    uint8_t capability_count;
    pci_capability_t capabilities[PCI_MAX_CAPABILITIES];
} pci_device_t;

void pci_init(void);
uint32_t pci_device_count(void);
const pci_device_t *pci_device(uint32_t index);
const pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass, uint32_t occurrence);
const pci_capability_t *pci_find_capability(const pci_device_t *device, uint8_t id, uint32_t occurrence);
