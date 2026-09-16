#include "gpu.h"

#include "console.h"
#include "pci.h"

#define PCI_CLASS_DISPLAY 0x03
#define PCI_SUBCLASS_VGA 0x00
#define PCI_SUBCLASS_3D 0x02
#define PCI_VENDOR_VIRTIO 0x1af4
#define PCI_VIRTIO_GPU_LEGACY 0x1040
#define PCI_VIRTIO_GPU_MODERN 0x1050
#define PCI_CAPABILITY_VIRTIO 0x09
#define VIRTIO_CAP_COMMON 1
#define VIRTIO_CAP_NOTIFY 2
#define VIRTIO_CAP_DEVICE 4
#define PCI_BAR_MEMORY_MASK 0xfffffff0u
#define PCI_BAR_MEMORY_64 0x04

static gpu_info_t device;
static uint8_t present;
static uint64_t frames;
static uint64_t frame_window_start;
static uint64_t frame_window_count;
static uint32_t frame_rate;

static uint64_t memory_bar(const pci_device_t *entry)
{
    if (entry == 0)
    {
        return 0;
    }
    for (uint8_t index = 0; index < 6; ++index)
    {
        uint32_t value = entry->bars[index];
        if ((value & 1u) == 0 && value != 0)
        {
            uint64_t address = value & PCI_BAR_MEMORY_MASK;
            if ((value & PCI_BAR_MEMORY_64) != 0 && index < 5)
            {
                address |= (uint64_t)entry->bars[index + 1] << 32;
            }
            return address;
        }
    }
    return 0;
}

void gpu_init(void)
{
    present = 0;
    frames = 0;
    frame_window_start = 0;
    frame_window_count = 0;
    frame_rate = 0;
    device = (gpu_info_t){0, 0, 0, GPU_NONE, 0, 0, 0};
    const pci_device_t *entry = 0;
    gpu_kind_t kind = GPU_NONE;
    for (uint32_t occurrence = 0; occurrence < 16 && entry == 0; ++occurrence)
    {
        const pci_device_t *candidate = pci_find_class(PCI_CLASS_DISPLAY, PCI_SUBCLASS_VGA, occurrence);
        if (candidate != 0)
        {
            entry = candidate;
            kind = GPU_DISPLAY;
        }
    }
    for (uint32_t index = 0; index < pci_device_count() && entry == 0; ++index)
    {
        const pci_device_t *candidate = pci_device(index);
        if (candidate != 0 && candidate->vendor_id == PCI_VENDOR_VIRTIO &&
            (candidate->device_id == PCI_VIRTIO_GPU_LEGACY ||
             candidate->device_id == PCI_VIRTIO_GPU_MODERN))
        {
            entry = candidate;
            kind = GPU_VIRTIO_CANDIDATE;
        }
    }
    for (uint32_t occurrence = 0; occurrence < 16 && entry == 0; ++occurrence)
    {
        const pci_device_t *candidate = pci_find_class(PCI_CLASS_DISPLAY, PCI_SUBCLASS_3D, occurrence);
        if (candidate != 0)
        {
            entry = candidate;
            kind = GPU_ACCELERATION_CANDIDATE;
        }
    }
    if (entry == 0)
    {
        console_write("gpu: display controller not found\n");
        return;
    }

    device.vendor_id = entry->vendor_id;
    device.device_id = entry->device_id;
    device.mmio_base = memory_bar(entry);
    device.kind = kind;
    device.acceleration_ready = 0;
    device.virtio_backend = kind == GPU_VIRTIO_CANDIDATE;
    device.virtio_transport_ready = 0;
    if (device.virtio_backend)
    {
        const pci_capability_t *common = 0;
        const pci_capability_t *notify = 0;
        const pci_capability_t *device_config = 0;
        for (uint8_t index = 0; index < entry->capability_count; ++index)
        {
            const pci_capability_t *capability = &entry->capabilities[index];
            if (capability->id != PCI_CAPABILITY_VIRTIO)
            {
                continue;
            }
            if (capability->type == VIRTIO_CAP_COMMON)
            {
                common = capability;
            }
            else if (capability->type == VIRTIO_CAP_NOTIFY)
            {
                notify = capability;
            }
            else if (capability->type == VIRTIO_CAP_DEVICE)
            {
                device_config = capability;
            }
        }
        device.virtio_transport_ready = common != 0 && notify != 0 && device_config != 0;
    }
    present = 1;
    console_write("gpu: display controller vendor=");
    console_write_hex(device.vendor_id);
    console_write(" device=");
    console_write_hex(device.device_id);
    console_write(" mmio=");
    console_write_hex(device.mmio_base);
    console_write(device.virtio_backend ? (device.virtio_transport_ready
                                               ? " virtio-backend=READY 3d-driver=NOT-CONNECTED\n"
                                               : " virtio-backend=DISCOVERED 3d-driver=NOT-CONNECTED\n")
                                        : " 3d-driver=NOT-CONNECTED\n");
}

void gpu_frame_present(uint64_t scheduler_tick)
{
    ++frames;
    if (frame_window_start == 0)
    {
        frame_window_start = scheduler_tick;
    }
    ++frame_window_count;
    if (scheduler_tick - frame_window_start >= 100)
    {
        frame_rate = (uint32_t)frame_window_count;
        frame_window_start = scheduler_tick;
        frame_window_count = 0;
    }
}

int gpu_present(void)
{
    return present != 0;
}

int gpu_acceleration_ready(void)
{
    return present != 0 && device.acceleration_ready != 0;
}

int gpu_virtio_present(void)
{
    return present != 0 && device.virtio_backend != 0;
}

const gpu_info_t *gpu_info(void)
{
    return present ? &device : 0;
}

uint64_t gpu_frame_count(void)
{
    return frames;
}

uint32_t gpu_frame_rate(void)
{
    return frame_rate;
}
