#include "acpi.h"
#include "console.h"

#define RSDP_SCAN_START 0x000e0000u
#define RSDP_SCAN_END 0x00100000u
#define RSDP_V1_LENGTH 20
#define RSDP_V2_LENGTH_OFFSET 20
#define RSDP_V2_REVISION 2
#define ACPI_MAX_TABLE_LENGTH (1024 * 1024)
#define ACPI_MAX_IOAPICS 16

static uintptr_t rsdp_address;
static uint8_t rsdp_revision;
static uint32_t local_apic_address;
static uint32_t ioapic_count;
static uint32_t processor_count;
static acpi_ioapic_t ioapics[ACPI_MAX_IOAPICS];

static int checksum_valid(const uint8_t *bytes, uint32_t length)
{
    uint8_t checksum = 0;
    for (uint32_t index = 0; index < length; ++index)
    {
        checksum = (uint8_t)(checksum + bytes[index]);
    }
    return checksum == 0;
}

static int signature_matches(const uint8_t *bytes)
{
    static const char signature[] = "RSD PTR ";
    for (uint8_t index = 0; index < 8; ++index)
    {
        if (bytes[index] != (uint8_t)signature[index])
        {
            return 0;
        }
    }
    return 1;
}

static int accept_rsdp(uintptr_t address)
{
    const uint8_t *candidate = (const uint8_t *)address;
    if (!signature_matches(candidate) || !checksum_valid(candidate, RSDP_V1_LENGTH))
    {
        return 0;
    }
    uint8_t revision = candidate[15];
    if (revision >= RSDP_V2_REVISION)
    {
        uint32_t length = (uint32_t)candidate[RSDP_V2_LENGTH_OFFSET] |
                          ((uint32_t)candidate[RSDP_V2_LENGTH_OFFSET + 1] << 8) |
                          ((uint32_t)candidate[RSDP_V2_LENGTH_OFFSET + 2] << 16) |
                          ((uint32_t)candidate[RSDP_V2_LENGTH_OFFSET + 3] << 24);
        if (length < RSDP_V1_LENGTH || length > ACPI_MAX_TABLE_LENGTH ||
            !checksum_valid(candidate, length))
        {
            return 0;
        }
    }
    rsdp_address = address;
    rsdp_revision = revision;
    return 1;
}

static int table_signature_matches(const uint8_t *bytes, const char *signature)
{
    for (uint8_t index = 0; index < 4; ++index)
    {
        if (bytes[index] != (uint8_t)signature[index])
        {
            return 0;
        }
    }
    return 1;
}

static uint32_t read32(const uint8_t *bytes, uint32_t offset)
{
    return (uint32_t)bytes[offset] | ((uint32_t)bytes[offset + 1] << 8) |
           ((uint32_t)bytes[offset + 2] << 16) | ((uint32_t)bytes[offset + 3] << 24);
}

static uint64_t read64(const uint8_t *bytes, uint32_t offset)
{
    return (uint64_t)read32(bytes, offset) | ((uint64_t)read32(bytes, offset + 4) << 32);
}

static const uint8_t *find_table(uint64_t root_address, uint8_t entry_size)
{
    if (root_address == 0)
    {
        return 0;
    }
    const uint8_t *root = (const uint8_t *)(uintptr_t)root_address;
    uint32_t length = read32(root, 4);
    if (length < 36 || length > ACPI_MAX_TABLE_LENGTH || !checksum_valid(root, length))
    {
        return 0;
    }
    uint32_t entry_count = (length - 36) / entry_size;
    for (uint32_t index = 0; index < entry_count; ++index)
    {
        uint64_t table_address = entry_size == 8 ? read64(root, 36 + index * entry_size)
                                                 : read32(root, 36 + index * entry_size);
        if (table_address == 0)
        {
            continue;
        }
        const uint8_t *table = (const uint8_t *)(uintptr_t)table_address;
        uint32_t table_length = read32(table, 4);
        if (table_length >= 36 && table_length <= ACPI_MAX_TABLE_LENGTH &&
            table_signature_matches(table, "APIC") &&
            checksum_valid(table, table_length))
        {
            return table;
        }
    }
    return 0;
}

static void parse_madt(const uint8_t *madt)
{
    local_apic_address = read32(madt, 36);
    uint32_t length = read32(madt, 4);
    uint32_t offset = 44;
    while (offset + 2 <= length)
    {
        uint8_t type = madt[offset];
        uint8_t entry_length = madt[offset + 1];
        if (entry_length < 2 || offset + entry_length > length)
        {
            break;
        }
        if (type == 0 && entry_length >= 8 && (madt[offset + 4] & 1) != 0)
        {
            ++processor_count;
        }
        else if (type == 1 && entry_length >= 12 && ioapic_count < ACPI_MAX_IOAPICS)
        {
            ioapics[ioapic_count].id = madt[offset + 2];
            ioapics[ioapic_count].address = read32(madt, offset + 4);
            ioapics[ioapic_count].gsi_base = read32(madt, offset + 8);
            ++ioapic_count;
        }
        offset += entry_length;
    }
}

static void discover_madt(const uint8_t *rsdp)
{
    uint64_t root_address;
    uint8_t entry_size;
    if (rsdp_revision >= RSDP_V2_REVISION)
    {
        root_address = read64(rsdp, 24);
        entry_size = 8;
    }
    else
    {
        root_address = read32(rsdp, 16);
        entry_size = 4;
    }
    const uint8_t *madt = find_table(root_address, entry_size);
    if (madt == 0)
    {
        console_write("acpi: MADT not found\n");
        return;
    }
    processor_count = 0;
    ioapic_count = 0;
    for (uint32_t index = 0; index < ACPI_MAX_IOAPICS; ++index)
    {
        ioapics[index] = (acpi_ioapic_t){0, 0, 0};
    }
    parse_madt(madt);
    console_write("acpi: MADT LAPIC ");
    console_write_hex(local_apic_address);
    console_write(" CPUs ");
    console_write_dec(processor_count);
    console_write(" IOAPICs ");
    console_write_dec(ioapic_count);
    console_write("\n");
    for (uint32_t index = 0; index < ioapic_count; ++index)
    {
        console_write("acpi: IOAPIC id ");
        console_write_dec(ioapics[index].id);
        console_write(" MMIO ");
        console_write_hex(ioapics[index].address);
        console_write(" GSI ");
        console_write_dec(ioapics[index].gsi_base);
        console_write("\n");
    }
}

void acpi_init(void)
{
    rsdp_address = 0;
    rsdp_revision = 0;
    local_apic_address = 0;
    processor_count = 0;
    ioapic_count = 0;
    uint16_t ebda_segment = *(const volatile uint16_t *)(uintptr_t)0x40e;
    uintptr_t ebda_start = (uintptr_t)ebda_segment << 4;
    uintptr_t ebda_end = ebda_start + 1024;
    if (ebda_start >= 0x80000 && ebda_end <= 0xa0000)
    {
        for (uintptr_t address = ebda_start; address < ebda_end; address += 16)
        {
            if (!accept_rsdp(address))
            {
                continue;
            }
            const uint8_t *candidate = (const uint8_t *)address;
            console_write("acpi: RSDP revision ");
            console_write_dec(rsdp_revision);
            console_write(" at ");
            console_write_hex(address);
            console_write("\n");
            discover_madt(candidate);
            return;
        }
    }
    for (uintptr_t address = RSDP_SCAN_START; address < RSDP_SCAN_END; address += 16)
    {
        if (!accept_rsdp(address))
        {
            continue;
        }
        const uint8_t *candidate = (const uint8_t *)address;
        console_write("acpi: RSDP revision ");
        console_write_dec(rsdp_revision);
        console_write(" at ");
        console_write_hex(address);
        console_write("\n");
        discover_madt(candidate);
        return;
    }
    console_write("acpi: RSDP not found\n");
}

int acpi_available(void)
{
    return rsdp_address != 0;
}

uintptr_t acpi_rsdp_address(void)
{
    return rsdp_address;
}

int acpi_madt_available(void)
{
    return local_apic_address != 0;
}

uint32_t acpi_local_apic_address(void)
{
    return local_apic_address;
}

uint32_t acpi_ioapic_count(void)
{
    return ioapic_count;
}

const acpi_ioapic_t *acpi_ioapic(uint32_t index)
{
    return index < ioapic_count ? &ioapics[index] : 0;
}
