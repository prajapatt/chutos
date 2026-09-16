#pragma once

#include <stdint.h>

typedef enum
{
    GPU_NONE = 0,
    GPU_DISPLAY = 1,
    GPU_ACCELERATION_CANDIDATE = 2,
    GPU_VIRTIO_CANDIDATE = 3
} gpu_kind_t;

typedef struct
{
    uint16_t vendor_id;
    uint16_t device_id;
    uint64_t mmio_base;
    gpu_kind_t kind;
    uint8_t acceleration_ready;
    uint8_t virtio_backend;
    uint8_t virtio_transport_ready;
} gpu_info_t;

void gpu_init(void);
void gpu_frame_present(uint64_t scheduler_tick);
int gpu_present(void);
int gpu_acceleration_ready(void);
int gpu_virtio_present(void);
const gpu_info_t *gpu_info(void);
uint64_t gpu_frame_count(void);
uint32_t gpu_frame_rate(void);
