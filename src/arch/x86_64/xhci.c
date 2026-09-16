#include "xhci.h"

#include "console.h"
#include "memory.h"
#include "pci.h"

#define PCI_CLASS_SERIAL_BUS 0x0c
#define PCI_SUBCLASS_USB 0x03
#define PCI_PROG_XHCI 0x30
#define PCI_BAR_MEMORY_MASK 0xfffffff0u
#define PCI_BAR_MEMORY_64 0x04
#define XHCI_USBCMD 0x00
#define XHCI_USBSTS 0x04
#define XHCI_USBSTS_HALTED 0x01
#define XHCI_USBCMD_RUN 0x01
#define XHCI_USBCMD_RESET 0x02
#define XHCI_USBCMD_INTERRUPTS 0x04
#define XHCI_CRCR 0x18
#define XHCI_DCBAAP 0x30
#define XHCI_CONFIG 0x38
#define XHCI_RUNTIME_INTERRUPTER 0x20
#define XHCI_IMAN 0x00
#define XHCI_ERSTSZ 0x08
#define XHCI_ERSTBA 0x10
#define XHCI_ERDP 0x18
#define XHCI_TRB_LINK_TYPE (6ULL << 10)
#define XHCI_TRB_ENABLE_SLOT_TYPE (9ULL << 10)
#define XHCI_TRB_ADDRESS_DEVICE_TYPE (11ULL << 10)
#define XHCI_TRB_EVENT_TYPE_MASK (0x3fULL << 10)
#define XHCI_TRB_COMMAND_COMPLETION_TYPE (33ULL << 10)
#define XHCI_TRB_CYCLE 0x01ULL
#define XHCI_TRB_COMPLETION_CODE_MASK 0xff000000u
#define XHCI_TRB_SLOT_ID_MASK 0xff000000u
#define XHCI_PORT_REGISTER_BASE 0x400
#define XHCI_PORT_REGISTER_STRIDE 0x10
#define XHCI_PORTSC_CONNECTED 0x01
#define XHCI_PORTSC_ENABLED 0x02
#define XHCI_PORTSC_RESET 0x10
#define XHCI_PORTSC_SPEED_SHIFT 10
#define XHCI_PORTSC_SPEED_MASK 0x3c00
#define XHCI_HCCPARAMS1 0x10
#define XHCI_EP0_TYPE_CONTROL 4ULL
#define XHCI_WAIT_LIMIT 1000000u

static xhci_info_t controller;
static uint8_t present;
static uint8_t ready;
static uint8_t rings_ready;
static uint64_t *command_ring;
static uint64_t *event_ring;
static uint64_t event_index;
static uint64_t *dcbaa;
static uint64_t *input_context;
static uint64_t *output_context;
static uint64_t *ep0_transfer_ring;

static uint32_t mmio_read32(uint64_t base, uint32_t offset)
{
    return *(volatile uint32_t *)(uintptr_t)(base + offset);
}

static void mmio_write32(uint64_t base, uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)(base + offset) = value;
}

static uint64_t pci_memory_bar(const pci_device_t *device)
{
    if (device == 0)
    {
        return 0;
    }
    for (uint8_t bar = 0; bar < 6; ++bar)
    {
        uint32_t value = device->bars[bar];
        if ((value & 1u) == 0 && value != 0)
        {
            uint64_t address = value & PCI_BAR_MEMORY_MASK;
            if ((value & PCI_BAR_MEMORY_64) != 0 && bar < 5)
            {
                address |= (uint64_t)device->bars[bar + 1] << 32;
            }
            return address;
        }
    }
    return 0;
}

static int wait_halted(uint64_t base)
{
    for (uint32_t attempt = 0; attempt < XHCI_WAIT_LIMIT; ++attempt)
    {
        if ((mmio_read32(base, XHCI_USBSTS) & XHCI_USBSTS_HALTED) != 0)
        {
            return 0;
        }
    }
    return -1;
}

void xhci_init(void)
{
    present = 0;
    ready = 0;
    controller = (xhci_info_t){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    rings_ready = 0;
    command_ring = 0;
    event_ring = 0;
    event_index = 0;
    dcbaa = 0;
    input_context = 0;
    output_context = 0;
    ep0_transfer_ring = 0;

    const pci_device_t *device = 0;
    for (uint32_t occurrence = 0; occurrence < 16; ++occurrence)
    {
        const pci_device_t *candidate =
            pci_find_class(PCI_CLASS_SERIAL_BUS, PCI_SUBCLASS_USB, occurrence);
        if (candidate == 0)
        {
            break;
        }
        if (candidate->programming_interface == PCI_PROG_XHCI)
        {
            device = candidate;
            break;
        }
    }
    if (device == 0)
    {
        console_write("xhci: controller not found\n");
        return;
    }

    uint64_t base = pci_memory_bar(device);
    if (base == 0 || base >= (1ULL << 30))
    {
        console_write("xhci: controller BAR is not identity-mapped\n");
        return;
    }

    uint32_t capability = *(volatile uint32_t *)(uintptr_t)base;
    uint8_t capability_length = (uint8_t)(capability & 0xff);
    uint16_t version = (uint16_t)(capability >> 16);
    uint32_t structural = mmio_read32(base, 0x04);
    uint32_t max_slots = structural & 0xff;
    uint32_t max_ports = (structural >> 24) & 0xff;
    uint32_t doorbell_offset = mmio_read32(base, 0x14);
    uint32_t runtime_offset = mmio_read32(base, 0x18);

    if (capability_length < 0x20 || max_slots == 0 || max_ports == 0)
    {
        console_write("xhci: invalid capability registers\n");
        return;
    }

    present = 1;
    controller = (xhci_info_t){base, capability_length, version, max_slots, max_ports,
                               doorbell_offset, runtime_offset, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t command = mmio_read32(base + capability_length, XHCI_USBCMD);
    command &= ~XHCI_USBCMD_RUN;
    mmio_write32(base + capability_length, XHCI_USBCMD, command);
    if (wait_halted(base + capability_length) != 0)
    {
        console_write("xhci: controller did not halt\n");
        return;
    }

    mmio_write32(base + capability_length, XHCI_USBCMD, XHCI_USBCMD_RESET);
    for (uint32_t attempt = 0; attempt < XHCI_WAIT_LIMIT; ++attempt)
    {
        if ((mmio_read32(base + capability_length, XHCI_USBCMD) & XHCI_USBCMD_RESET) == 0)
        {
            ready = 1;
            break;
        }
    }

    if (!ready)
    {
        return;
    }

    dcbaa = (uint64_t *)memory_alloc_page();
    command_ring = (uint64_t *)memory_alloc_page();
    event_ring = (uint64_t *)memory_alloc_page();
    uint64_t *segment_table = (uint64_t *)memory_alloc_page();
    if (dcbaa == 0 || command_ring == 0 || event_ring == 0 || segment_table == 0)
    {
        console_write("xhci: DMA ring allocation failed\n");
        return;
    }

    command_ring[510] = (uint64_t)(uintptr_t)command_ring;
    command_ring[511] = (XHCI_TRB_CYCLE | XHCI_TRB_LINK_TYPE) << 32;
    segment_table[0] = (uint64_t)(uintptr_t)event_ring;
    segment_table[1] = 256;
    uint64_t operational_base = base + capability_length;
    uint64_t runtime_base = base + runtime_offset;
    mmio_write32(operational_base, XHCI_DCBAAP, (uint32_t)(uintptr_t)dcbaa);
    mmio_write32(operational_base, XHCI_DCBAAP + 4, (uint32_t)((uint64_t)(uintptr_t)dcbaa >> 32));
    mmio_write32(operational_base, XHCI_CONFIG, max_slots);
    mmio_write32(operational_base, XHCI_CRCR, (uint32_t)(uintptr_t)command_ring | 1);
    mmio_write32(operational_base, XHCI_CRCR + 4, (uint32_t)((uint64_t)(uintptr_t)command_ring >> 32));
    mmio_write32(runtime_base + XHCI_RUNTIME_INTERRUPTER, XHCI_ERSTSZ, 1);
    mmio_write32(runtime_base + XHCI_RUNTIME_INTERRUPTER, XHCI_ERSTBA,
                 (uint32_t)(uintptr_t)segment_table);
    mmio_write32(runtime_base + XHCI_RUNTIME_INTERRUPTER, XHCI_ERSTBA + 4,
                 (uint32_t)((uint64_t)(uintptr_t)segment_table >> 32));
    mmio_write32(runtime_base + XHCI_RUNTIME_INTERRUPTER, XHCI_ERDP, (uint32_t)(uintptr_t)event_ring);
    mmio_write32(runtime_base + XHCI_RUNTIME_INTERRUPTER, XHCI_ERDP + 4,
                 (uint32_t)((uint64_t)(uintptr_t)event_ring >> 32));
    mmio_write32(runtime_base + XHCI_RUNTIME_INTERRUPTER, XHCI_IMAN, 0x02);
    mmio_write32(operational_base, XHCI_USBCMD, XHCI_USBCMD_RUN | XHCI_USBCMD_INTERRUPTS);
    rings_ready = 1;
    controller.command_ring = (uint64_t)(uintptr_t)command_ring;
    controller.event_ring = (uint64_t)(uintptr_t)event_ring;

    uint32_t connected_ports = 0;
    for (uint32_t port = 0; port < max_ports; ++port)
    {
        uint32_t port_status = mmio_read32(base + capability_length,
                                           XHCI_PORT_REGISTER_BASE + port * XHCI_PORT_REGISTER_STRIDE);
        if ((port_status & XHCI_PORTSC_CONNECTED) != 0)
        {
            ++connected_ports;
        }
    }
    controller.connected_ports = connected_ports;

    console_write("xhci: USB controller ");
    console_write(ready ? "ready" : "reset timeout");
    console_write(" version=");
    console_write_hex(version);
    console_write(" slots=");
    console_write_dec(max_slots);
    console_write(" ports=");
    console_write_dec(max_ports);
    console_write(" connected=");
    console_write_dec(connected_ports);
    console_write("\n");
    if (connected_ports != 0)
    {
        if (xhci_reset_connected_ports() == 0 && xhci_enable_slot() == 0)
        {
            console_write("xhci: USB device slot enabled slot=");
            console_write_dec(controller.enabled_slot);
            console_write("\n");
            if (xhci_prepare_device_context() != 0)
            {
                console_write("xhci: device context preparation failed\n");
            }
            else if (xhci_address_device() != 0)
            {
                console_write("xhci: address-device command failed or timed out\n");
            }
        }
        else
        {
            console_write("xhci: enable-slot command failed or timed out\n");
        }
    }
}

int xhci_present(void)
{
    return present != 0;
}

int xhci_ready(void)
{
    return ready != 0;
}

const xhci_info_t *xhci_info(void)
{
    return present ? &controller : 0;
}

uint32_t xhci_connected_ports(void)
{
    return controller.connected_ports;
}

int xhci_rings_ready(void)
{
    return rings_ready != 0;
}

int xhci_enable_slot(void)
{
    if (!rings_ready || controller.connected_ports == 0 || controller.enabled_slot != 0)
    {
        return controller.enabled_slot != 0 ? 0 : -1;
    }

    command_ring[0] = 0;
    command_ring[1] = (XHCI_TRB_CYCLE | XHCI_TRB_ENABLE_SLOT_TYPE) << 32;
    __asm__ volatile("mfence" : : : "memory");
    volatile uint32_t *doorbell = (volatile uint32_t *)(uintptr_t)(controller.mmio_base +
                                                                   controller.doorbell_offset);
    doorbell[0] = 0;

    for (uint32_t attempt = 0; attempt < XHCI_WAIT_LIMIT; ++attempt)
    {
        uint32_t event_status = (uint32_t)event_ring[event_index * 2 + 1];
        uint32_t event_control = (uint32_t)(event_ring[event_index * 2 + 1] >> 32);
        if ((event_control & XHCI_TRB_CYCLE) == 0)
        {
            continue;
        }
        uint32_t event_type = event_control & (uint32_t)XHCI_TRB_EVENT_TYPE_MASK;
        event_ring[event_index * 2 + 1] &= ~((uint64_t)XHCI_TRB_CYCLE << 32);
        event_index = (event_index + 1) % 256;
        if (event_type != XHCI_TRB_COMMAND_COMPLETION_TYPE)
        {
            continue;
        }
        uint8_t completion_code = (uint8_t)((event_status & XHCI_TRB_COMPLETION_CODE_MASK) >> 24);
        uint8_t slot = (uint8_t)((event_control & XHCI_TRB_SLOT_ID_MASK) >> 24);
        if (completion_code == 1 && slot != 0 && slot <= controller.max_slots)
        {
            controller.enabled_slot = slot;
            return 0;
        }
        return -1;
    }
    return -1;
}

int xhci_prepare_device_context(void)
{
    if (!rings_ready || controller.enabled_slot == 0 || controller.active_port == 0 ||
        controller.context_ready != 0)
    {
        return controller.context_ready != 0 ? 0 : -1;
    }

    uint32_t capability = mmio_read32(controller.mmio_base, XHCI_HCCPARAMS1);
    uint8_t context_size = (capability & 0x04) != 0 ? 64 : 32;
    input_context = (uint64_t *)memory_alloc_page();
    output_context = (uint64_t *)memory_alloc_page();
    ep0_transfer_ring = (uint64_t *)memory_alloc_page();
    if (input_context == 0 || output_context == 0 || ep0_transfer_ring == 0)
    {
        return -1;
    }

    uint64_t operational_base = controller.mmio_base + controller.capability_length;
    uint32_t port_status = mmio_read32(operational_base,
                                       XHCI_PORT_REGISTER_BASE +
                                           (controller.active_port - 1) * XHCI_PORT_REGISTER_STRIDE);
    uint8_t speed = (uint8_t)((port_status & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT);
    uint16_t max_packet = speed >= 4 ? 512 : (speed == 1 ? 8 : 64);
    uint64_t slot_offset = 4 + context_size / 8;
    uint64_t endpoint_offset = slot_offset + context_size / 8;
    input_context[1] = 0x03;
    input_context[slot_offset] = ((uint64_t)speed << 20) | ((uint64_t)controller.active_port << 48);
    input_context[endpoint_offset] = ((uint64_t)max_packet << 48) |
                                     (XHCI_EP0_TYPE_CONTROL << 35);
    input_context[endpoint_offset + 1] = (uint64_t)(uintptr_t)ep0_transfer_ring | 1;
    ep0_transfer_ring[510] = (uint64_t)(uintptr_t)ep0_transfer_ring;
    ep0_transfer_ring[511] = (XHCI_TRB_CYCLE | XHCI_TRB_LINK_TYPE) << 32;
    dcbaa[controller.enabled_slot] = (uint64_t)(uintptr_t)output_context;
    __asm__ volatile("mfence" : : : "memory");
    controller.context_size = context_size;
    controller.context_ready = 1;
    return 0;
}

int xhci_address_device(void)
{
    if (!rings_ready || !controller.context_ready || controller.enabled_slot == 0 ||
        controller.address_ready != 0)
    {
        return controller.address_ready != 0 ? 0 : -1;
    }

    command_ring[0] = (uint64_t)(uintptr_t)input_context;
    command_ring[1] = ((uint64_t)controller.enabled_slot << 24 |
                       XHCI_TRB_CYCLE | XHCI_TRB_ADDRESS_DEVICE_TYPE)
                      << 32;
    __asm__ volatile("mfence" : : : "memory");
    volatile uint32_t *doorbell = (volatile uint32_t *)(uintptr_t)(controller.mmio_base +
                                                                   controller.doorbell_offset);
    doorbell[0] = 0;

    for (uint32_t attempt = 0; attempt < XHCI_WAIT_LIMIT; ++attempt)
    {
        uint32_t event_status = (uint32_t)event_ring[event_index * 2 + 1];
        uint32_t event_control = (uint32_t)(event_ring[event_index * 2 + 1] >> 32);
        if ((event_control & XHCI_TRB_CYCLE) == 0)
        {
            continue;
        }
        uint32_t event_type = event_control & (uint32_t)XHCI_TRB_EVENT_TYPE_MASK;
        uint8_t slot = (uint8_t)((event_control & XHCI_TRB_SLOT_ID_MASK) >> 24);
        event_ring[event_index * 2 + 1] &= ~((uint64_t)XHCI_TRB_CYCLE << 32);
        event_index = (event_index + 1) % 256;
        if (event_type != XHCI_TRB_COMMAND_COMPLETION_TYPE)
        {
            continue;
        }
        uint8_t completion_code = (uint8_t)((event_status & XHCI_TRB_COMPLETION_CODE_MASK) >> 24);
        if (completion_code != 1 || slot != controller.enabled_slot)
        {
            return -1;
        }
        controller.address_ready = 1;
        return 0;
    }
    return -1;
}

int xhci_reset_connected_ports(void)
{
    if (!ready || !rings_ready)
    {
        return -1;
    }

    uint64_t operational_base = controller.mmio_base + controller.capability_length;
    for (uint32_t port = 0; port < controller.max_ports; ++port)
    {
        uint32_t offset = XHCI_PORT_REGISTER_BASE + port * XHCI_PORT_REGISTER_STRIDE;
        uint32_t status = mmio_read32(operational_base, offset);
        if ((status & XHCI_PORTSC_CONNECTED) == 0)
        {
            continue;
        }

        mmio_write32(operational_base, offset, status | XHCI_PORTSC_RESET);
        for (uint32_t attempt = 0; attempt < XHCI_WAIT_LIMIT; ++attempt)
        {
            status = mmio_read32(operational_base, offset);
            if ((status & XHCI_PORTSC_RESET) == 0)
            {
                if ((status & XHCI_PORTSC_ENABLED) != 0)
                {
                    controller.active_port = port + 1;
                    return 0;
                }
                break;
            }
        }
    }
    return -1;
}
