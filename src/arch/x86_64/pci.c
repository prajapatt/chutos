#include "pci.h"
#include "console.h"
#include "port.h"

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA 0xcfc
#define PCI_MAX_DEVICES 256

static uint32_t discovered_devices;
static pci_device_t devices[PCI_MAX_DEVICES];

static uint32_t config_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    return 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)device << 11) |
           ((uint32_t)function << 8) | (offset & 0xfcu);
}

static uint32_t read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    outl(PCI_CONFIG_ADDRESS, config_address(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

static void read_capabilities(uint8_t bus, uint8_t device, uint8_t function, pci_device_t *entry)
{
    uint32_t header = read_config(bus, device, function, 0x0c);
    if (((header >> 16) & 0x7f) != 0)
    {
        return;
    }

    uint8_t offset = (uint8_t)(read_config(bus, device, function, 0x34) & 0xff);
    for (uint8_t steps = 0; offset >= 0x40 && offset <= 0xf4 && steps < PCI_MAX_CAPABILITIES;
         ++steps)
    {
        uint32_t capability = read_config(bus, device, function, offset);
        uint8_t id = (uint8_t)capability;
        uint8_t next = (uint8_t)(capability >> 8);
        if (entry->capability_count < PCI_MAX_CAPABILITIES)
        {
            pci_capability_t *record = &entry->capabilities[entry->capability_count++];
            record->id = id;
            record->offset = offset;
            record->type = (uint8_t)(capability >> 16);
            record->bar = (uint8_t)(capability >> 24);
            record->bar_offset = read_config(bus, device, function, (uint8_t)(offset + 4));
            record->length = read_config(bus, device, function, (uint8_t)(offset + 8));
        }
        if (next == 0 || next <= offset)
        {
            break;
        }
        offset = next;
    }
}

static void print_device(const pci_device_t *entry)
{
    console_write("pci ");
    console_write_hex(entry->bus);
    console_write(":");
    console_write_hex(entry->device);
    console_write(".");
    console_write_hex(entry->function);
    console_write(" vendor/device ");
    console_write_hex(entry->vendor_id);
    console_write("/");
    console_write_hex(entry->device_id);
    console_write(" class ");
    console_write_hex(entry->class_code);
    console_write_hex(entry->subclass);
    console_write("\n");
}

static void read_device(uint8_t bus, uint8_t device, uint8_t function, pci_device_t *entry)
{
    uint32_t identity = read_config(bus, device, function, 0x00);
    uint32_t class_code = read_config(bus, device, function, 0x08);
    uint32_t header = read_config(bus, device, function, 0x0c);
    *entry = (pci_device_t){
        bus,
        device,
        function,
        (uint8_t)(header >> 16),
        (uint16_t)identity,
        (uint16_t)(identity >> 16),
        (uint8_t)(class_code >> 24),
        (uint8_t)(class_code >> 16),
        (uint8_t)(class_code >> 8),
        {0, 0, 0, 0, 0, 0},
        0,
        {{0}},
    };
    for (uint8_t bar = 0; bar < 6; ++bar)
    {
        entry->bars[bar] = read_config(bus, device, function, (uint8_t)(0x10 + bar * 4));
    }
    read_capabilities(bus, device, function, entry);
}

static void print_bar_summary(const pci_device_t *entry)
{
    for (uint8_t bar = 0; bar < 6; ++bar)
    {
        if (entry->bars[bar] != 0)
        {
            console_write("pci: BAR");
            console_write_dec(bar);
            console_write(" ");
            console_write_hex(entry->bars[bar]);
            console_write("\n");
        }
    }
}

void pci_init(void)
{
    discovered_devices = 0;
    for (uint16_t bus = 0; bus < 256; ++bus)
    {
        for (uint8_t device = 0; device < 32; ++device)
        {
            uint32_t function_limit = 1;
            uint32_t header = read_config((uint8_t)bus, device, 0, 0x0c);
            if ((header & 0x00800000u) != 0)
            {
                function_limit = 8;
            }
            for (uint8_t function = 0; function < function_limit; ++function)
            {
                uint32_t identity = read_config((uint8_t)bus, device, function, 0x00);
                if ((identity & 0xffffu) == 0xffffu)
                {
                    continue;
                }
                if (discovered_devices < PCI_MAX_DEVICES)
                {
                    read_device((uint8_t)bus, device, function, &devices[discovered_devices]);
                    print_device(&devices[discovered_devices]);
                    print_bar_summary(&devices[discovered_devices]);
                }
                ++discovered_devices;
            }
        }
    }
    console_write("pci: discovered ");
    console_write_dec(discovered_devices);
    console_write(" device(s)\n");
}

uint32_t pci_device_count(void)
{
    return discovered_devices;
}

const pci_device_t *pci_device(uint32_t index)
{
    return index < discovered_devices && index < PCI_MAX_DEVICES ? &devices[index] : 0;
}

const pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass, uint32_t occurrence)
{
    for (uint32_t index = 0; index < discovered_devices && index < PCI_MAX_DEVICES; ++index)
    {
        const pci_device_t *entry = &devices[index];
        if (entry->class_code != class_code || entry->subclass != subclass)
        {
            continue;
        }
        if (occurrence == 0)
        {
            return entry;
        }
        --occurrence;
    }
    return 0;
}

const pci_capability_t *pci_find_capability(const pci_device_t *device, uint8_t id, uint32_t occurrence)
{
    if (device == 0)
    {
        return 0;
    }
    for (uint8_t index = 0; index < device->capability_count; ++index)
    {
        if (device->capabilities[index].id != id)
        {
            continue;
        }
        if (occurrence == 0)
        {
            return &device->capabilities[index];
        }
        --occurrence;
    }
    return 0;
}
