#pragma once

#include <stdint.h>

typedef struct
{
    uint64_t mmio_base;
    uint8_t capability_length;
    uint16_t version;
    uint32_t max_slots;
    uint32_t max_ports;
    uint32_t doorbell_offset;
    uint32_t runtime_offset;
    uint32_t connected_ports;
    uint32_t active_port;
    uint64_t command_ring;
    uint64_t event_ring;
    uint8_t enabled_slot;
    uint8_t context_size;
    uint8_t context_ready;
    uint8_t address_ready;
} xhci_info_t;

void xhci_init(void);
int xhci_present(void);
int xhci_ready(void);
const xhci_info_t *xhci_info(void);
uint32_t xhci_connected_ports(void);
int xhci_rings_ready(void);
int xhci_enable_slot(void);
int xhci_reset_connected_ports(void);
int xhci_prepare_device_context(void);
int xhci_address_device(void);
