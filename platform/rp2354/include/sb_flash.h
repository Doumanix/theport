#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint32_t SB_FLASH_STORAGE_SIZE = 16 * 1024;

uint32_t sb_flash_size_bytes();
uint32_t sb_flash_storage_offset();
uint32_t sb_flash_storage_size();

bool sb_flash_is_storage_range(uint32_t offset, size_t len);

void sb_flash_read(uint32_t offset, void* data, size_t len);
bool sb_flash_erase(uint32_t offset, size_t len);
bool sb_flash_write(uint32_t offset, const void* data, size_t len);

void sb_flash_storage_read(uint32_t offset, void* data, size_t len);
