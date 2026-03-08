#include "sb_flash.h"

#include <cstring>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/platform.h"

namespace {

constexpr uint32_t kFlashPageSize = FLASH_PAGE_SIZE;
constexpr uint32_t kFlashSectorSize = FLASH_SECTOR_SIZE;

bool is_range_valid(uint32_t offset, size_t len) {
    if (len == 0) {
        return true;
    }

    const uint64_t end = static_cast<uint64_t>(offset) + static_cast<uint64_t>(len);
    return end <= sb_flash_size_bytes();
}

}  // namespace

uint32_t sb_flash_size_bytes() {
#ifdef PICO_FLASH_SIZE_BYTES
    return PICO_FLASH_SIZE_BYTES;
#else
#error "PICO_FLASH_SIZE_BYTES is not defined for this board/target."
#endif
}

uint32_t sb_flash_storage_offset() {
    return sb_flash_size_bytes() - SB_FLASH_STORAGE_SIZE;
}

uint32_t sb_flash_storage_size() {
    return SB_FLASH_STORAGE_SIZE;
}

bool sb_flash_is_storage_range(uint32_t offset, size_t len) {
    if (!is_range_valid(offset, len)) {
        return false;
    }

    const uint32_t base = sb_flash_storage_offset();
    const uint64_t end = static_cast<uint64_t>(offset) + static_cast<uint64_t>(len);
    return offset >= base && end <= static_cast<uint64_t>(base) + SB_FLASH_STORAGE_SIZE;
}

void sb_flash_read(uint32_t offset, void* data, size_t len) {
    if (!data || !is_range_valid(offset, len)) {
        return;
    }

    const uint8_t* flash_ptr =
        reinterpret_cast<const uint8_t*>(XIP_BASE + static_cast<uintptr_t>(offset));
    std::memcpy(data, flash_ptr, len);
}

bool sb_flash_erase(uint32_t offset, size_t len) {
    if (!sb_flash_is_storage_range(offset, len)) {
        return false;
    }
    if ((offset % kFlashSectorSize) != 0 || (len % kFlashSectorSize) != 0) {
        return false;
    }

    const uint32_t irq_state = save_and_disable_interrupts();
    flash_range_erase(offset, len);
    restore_interrupts(irq_state);
    return true;
}

bool sb_flash_write(uint32_t offset, const void* data, size_t len) {
    if (!data || !sb_flash_is_storage_range(offset, len)) {
        return false;
    }
    if ((offset % kFlashPageSize) != 0 || (len % kFlashPageSize) != 0) {
        return false;
    }

    const uint32_t irq_state = save_and_disable_interrupts();
    flash_range_program(offset, reinterpret_cast<const uint8_t*>(data), len);
    restore_interrupts(irq_state);
    return true;
}

void sb_flash_storage_read(uint32_t offset, void* data, size_t len) {
    if (offset + len > SB_FLASH_STORAGE_SIZE) {
        return;
    }
    sb_flash_read(sb_flash_storage_offset() + offset, data, len);
}

bool sb_flash_storage_write(uint32_t offset, const void* data, size_t len)
{
    if (offset + len > SB_FLASH_STORAGE_SIZE) {
        return false;
    }
}