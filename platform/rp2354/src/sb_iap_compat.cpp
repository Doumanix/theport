#include <cstdint>
#include <cstring>

#include <sblib/platform.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico.h"

#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2u * 1024u * 1024u)
#endif

namespace
{
constexpr uint32_t RP_STORAGE_SIZE = 16u * 1024u;

static uint8_t page_buffer[FLASH_PAGE_SIZE] __attribute__((aligned(FLASH_PAGE_SIZE)));
static uint8_t sector_buffer[FLASH_SECTOR_SIZE] __attribute__((aligned(FLASH_PAGE_SIZE)));

static inline uintptr_t xip_base_addr()
{
    return (uintptr_t)XIP_BASE;
}

static inline uint32_t flash_size_bytes()
{
    return (uint32_t)PICO_FLASH_SIZE_BYTES;
}

static inline uint32_t storage_base_offset()
{
    return flash_size_bytes() - RP_STORAGE_SIZE;
}

static bool address_to_offset(const unsigned char* addr, uint32_t& offset)
{
    const uintptr_t a = (uintptr_t)addr;
    const uintptr_t base = xip_base_addr();
    const uintptr_t end = base + flash_size_bytes();

    if (a < base || a >= end)
        return false;

    offset = (uint32_t)(a - base);
    return true;
}

static bool is_storage_range(uint32_t offset, uint32_t size)
{
    if (size == 0)
        return false;

    const uint32_t storage_base = storage_base_offset();

    if (offset < storage_base)
        return false;

    if (offset > flash_size_bytes())
        return false;

    if (size > flash_size_bytes() - offset)
        return false;

    return true;
}

static bool verify_erased(uint32_t offset, uint32_t size)
{
    const uint8_t* p = (const uint8_t*)(xip_base_addr() + offset);

    for (uint32_t i = 0; i < size; ++i)
    {
        if (p[i] != 0xff)
            return false;
    }

    return true;
}

static bool verify_programmed(uint32_t offset, const uint8_t* data, uint32_t size)
{
    const uint8_t* p = (const uint8_t*)(xip_base_addr() + offset);
    return std::memcmp(p, data, size) == 0;
}
}

unsigned int iapFlashSize()
{
    return (unsigned int)flash_size_bytes();
}

int iapSectorOfAddress(const unsigned char* addr)
{
    uint32_t offset = 0;
    if (!address_to_offset(addr, offset))
        return -1;

    return (int)(offset / FLASH_SECTOR_SIZE);
}

unsigned int iapPageOfAddress(const unsigned char* addr)
{
    uint32_t offset = 0;

    if (!address_to_offset(addr, offset))
        return 0;

    return offset / FLASH_PAGE_SIZE;
}

int iapEraseSector(unsigned int sector)
{
    const uint32_t offset = (uint32_t)sector * FLASH_SECTOR_SIZE;

    if (!is_storage_range(offset, FLASH_SECTOR_SIZE))
        return -1;

    const uint32_t irq_state = save_and_disable_interrupts();
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    restore_interrupts(irq_state);

    return verify_erased(offset, FLASH_SECTOR_SIZE) ? 0 : -1;
}

int iapErasePage(unsigned int page)
{
    const uint32_t page_offset = (uint32_t)page * FLASH_PAGE_SIZE;

    if (!is_storage_range(page_offset, FLASH_PAGE_SIZE))
        return -1;

    const uint32_t sector_offset = page_offset & ~(uint32_t)(FLASH_SECTOR_SIZE - 1u);
    const uint32_t page_in_sector_offset = page_offset - sector_offset;

    const uint8_t* sector_xip = (const uint8_t*)(xip_base_addr() + sector_offset);
    std::memcpy(sector_buffer, sector_xip, FLASH_SECTOR_SIZE);
    std::memset(sector_buffer + page_in_sector_offset, 0xff, FLASH_PAGE_SIZE);

    uint32_t irq_state = save_and_disable_interrupts();
    flash_range_erase(sector_offset, FLASH_SECTOR_SIZE);
    restore_interrupts(irq_state);

    if (!verify_erased(sector_offset, FLASH_SECTOR_SIZE))
        return -1;

    for (uint32_t off = 0; off < FLASH_SECTOR_SIZE; off += FLASH_PAGE_SIZE)
    {
        irq_state = save_and_disable_interrupts();
        flash_range_program(sector_offset + off, sector_buffer + off, FLASH_PAGE_SIZE);
        restore_interrupts(irq_state);

        if (!verify_programmed(sector_offset + off, sector_buffer + off, FLASH_PAGE_SIZE))
            return -1;
    }

    return 0;
}

int iapProgram(unsigned char* dst, const unsigned char* src, unsigned int size)
{
    if (!dst || !src || size == 0)
        return -1;

    uint32_t dst_offset = 0;
    if (!address_to_offset(dst, dst_offset))
        return -1;

    if (!is_storage_range(dst_offset, size))
        return -1;

    uint32_t processed = 0;

    while (processed < size)
    {
        const uint32_t current_offset = dst_offset + processed;
        const uint32_t page_offset = current_offset & ~(uint32_t)(FLASH_PAGE_SIZE - 1u);
        const uint32_t offset_in_page = current_offset - page_offset;
        const uint32_t chunk = ((size - processed) < (FLASH_PAGE_SIZE - offset_in_page))
                             ? (size - processed)
                             : (FLASH_PAGE_SIZE - offset_in_page);

        const uint8_t* page_xip = (const uint8_t*)(xip_base_addr() + page_offset);
        std::memcpy(page_buffer, page_xip, FLASH_PAGE_SIZE);
        std::memcpy(page_buffer + offset_in_page, src + processed, chunk);

        const uint32_t irq_state = save_and_disable_interrupts();
        flash_range_program(page_offset, page_buffer, FLASH_PAGE_SIZE);
        restore_interrupts(irq_state);

        if (!verify_programmed(page_offset, page_buffer, FLASH_PAGE_SIZE))
            return -1;

        processed += chunk;
    }

    return 0;
}

unsigned char* iapAddressOfPage(unsigned int page)
{
    return (unsigned char*)(xip_base_addr() + (uintptr_t)page * FLASH_PAGE_SIZE);
}