#pragma once

#include <stdint.h>

typedef struct {
    uint32_t maximum_basic_leaf;
    uint32_t maximum_extended_leaf;
    uint8_t apic;
    uint8_t nx;
    uint8_t one_gib_pages;
    uint8_t invariant_tsc;
} cpu_features_t;

void cpu_init(void);
const cpu_features_t *cpu_features(void);
