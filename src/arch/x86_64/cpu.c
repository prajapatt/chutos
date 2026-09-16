#include "cpu.h"
#include "console.h"

static cpu_features_t features;

static void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx,
                  uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
}

void cpu_init(void)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    features.maximum_basic_leaf = eax;
    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    features.maximum_extended_leaf = eax;
    features.apic = 0;
    features.nx = 0;
    features.one_gib_pages = 0;
    features.invariant_tsc = 0;
    if (features.maximum_basic_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        features.apic = (uint8_t)((edx >> 9) & 1);
    }
    if (features.maximum_extended_leaf >= 0x80000001) {
        cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
        features.nx = (uint8_t)((edx >> 20) & 1);
        features.one_gib_pages = (uint8_t)((edx >> 26) & 1);
    }
    if (features.maximum_extended_leaf >= 0x80000007) {
        cpuid(0x80000007, 0, &eax, &ebx, &ecx, &edx);
        features.invariant_tsc = (uint8_t)((edx >> 8) & 1);
    }
    console_write("cpu: APIC ");
    console_write(features.apic ? "yes" : "no");
    console_write(" NX ");
    console_write(features.nx ? "yes" : "no");
    console_write(" 1GiB-pages ");
    console_write(features.one_gib_pages ? "yes" : "no");
    console_write(" invariant-TSC ");
    console_write(features.invariant_tsc ? "yes" : "no");
    console_write("\n");
}

const cpu_features_t *cpu_features(void)
{
    return &features;
}
