/*
 *  Copyright (c) 2014 Martin Glueck <martin@mangari.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <sblib/internal/iap.h>

#include <sblib/interrupt.h>
#include <sblib/platform.h>
#include <string.h>

#if defined(SBLIB_PLATFORM_RP2354) || defined(PICO_RP2350)

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/unique_id.h"

namespace
{
constexpr uint32_t RpStorageSize = 16u * 1024u;
constexpr unsigned int PagesPerSector = FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE;

__attribute__ ((aligned (FLASH_RAM_BUFFER_ALIGNMENT))) uint8_t sectorBuffer[FLASH_SECTOR_SIZE];

static_assert((RpStorageSize % FLASH_SECTOR_SIZE) == 0, "RP2354 storage area must be sector aligned");
static_assert((FLASH_SECTOR_SIZE % FLASH_PAGE_SIZE) == 0, "FLASH geometry mismatch");

inline uintptr_t flashBaseAddress()
{
    return reinterpret_cast<uintptr_t>(FLASH_BASE_ADDRESS);
}

inline uintptr_t flashEndAddress()
{
    return flashBaseAddress() + static_cast<uintptr_t>(iapFlashSize());
}

inline uint32_t storageBaseOffset()
{
    return iapFlashSize() - RpStorageSize;
}

inline bool addWillOverflow(const uint32_t base, const uint32_t len)
{
    return len > (0xffffffffu - base);
}

bool addressToFlashOffset(const uint8_t* address, uint32_t& offset)
{
    const uintptr_t addr = reinterpret_cast<uintptr_t>(address);
    if (addr < flashBaseAddress() || addr >= flashEndAddress())
    {
        return false;
    }

    offset = static_cast<uint32_t>(addr - flashBaseAddress());
    return true;
}

bool isStorageOffsetRange(const uint32_t offset, const uint32_t size)
{
    if (size == 0)
    {
        return false;
    }

    if (addWillOverflow(offset, size))
    {
        return false;
    }

    const uint32_t storageBase = storageBaseOffset();
    const uint32_t end = offset + size;
    return offset >= storageBase && end <= iapFlashSize();
}

bool isStorageAddressRange(const uint8_t* address, const uint32_t size)
{
    uint32_t offset = 0;
    if (!addressToFlashOffset(address, offset))
    {
        return false;
    }

    return isStorageOffsetRange(offset, size);
}

bool bufferIsErased(const uint8_t* data, const size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        if (data[i] != 0xff)
        {
            return false;
        }
    }
    return true;
}

IAP_Status eraseOffsetRange(const uint32_t offset, const uint32_t size)
{
    if ((offset % FLASH_SECTOR_SIZE) != 0 || (size % FLASH_SECTOR_SIZE) != 0)
    {
        return IAP_DST_ADDR_ERROR;
    }

    if (!isStorageOffsetRange(offset, size))
    {
        return IAP_INVALID_SECTOR;
    }

    const uint32_t irqState = save_and_disable_interrupts();
    flash_range_erase(offset, size);
    restore_interrupts(irqState);

    const uint8_t* flash = reinterpret_cast<const uint8_t*>(flashBaseAddress() + offset);
    return bufferIsErased(flash, size) ? IAP_SUCCESS : IAP_SECTOR_NOT_BLANK;
}

IAP_Status programOffsetRange(const uint32_t offset, const uint8_t* ram, const uint32_t size)
{
    if ((offset % FLASH_PAGE_SIZE) != 0)
    {
        return IAP_DST_ADDR_ERROR;
    }

    if ((reinterpret_cast<uintptr_t>(ram) % FLASH_RAM_BUFFER_ALIGNMENT) != 0)
    {
        return IAP_SRC_ADDR_ERROR;
    }

    if (size == 0 || (size % FLASH_PAGE_SIZE) != 0)
    {
        return IAP_COUNT_ERROR;
    }

    if (!isStorageOffsetRange(offset, size))
    {
        return IAP_DST_ADDR_NOT_MAPPED;
    }

    const uint32_t irqState = save_and_disable_interrupts();
    flash_range_program(offset, ram, size);
    restore_interrupts(irqState);

    const uint8_t* flash = reinterpret_cast<const uint8_t*>(flashBaseAddress() + offset);
    return memcmp(flash, ram, size) == 0 ? IAP_SUCCESS : IAP_COMPARE_ERROR;
}
}

// The size of the flash in bytes. Use iapFlashSize() to get the flash size.
unsigned int iapFlashBytes = 0;

IAP_Status iapEraseSector(const unsigned int sector)
{
    return iapEraseSectorRange(sector, sector);
}

IAP_Status iapEraseSectorRange(const unsigned int startSector, const unsigned int endSector)
{
    if (endSector < startSector)
    {
        return IAP_INVALID_SECTOR;
    }

    const uint32_t offset = startSector * FLASH_SECTOR_SIZE;
    const uint32_t size = (endSector - startSector + 1u) * FLASH_SECTOR_SIZE;
    return eraseOffsetRange(offset, size);
}

IAP_Status iapErasePage(const unsigned int pageNumber)
{
    return iapErasePageRange(pageNumber, pageNumber);
}

IAP_Status iapErasePageRange(const unsigned int startPageNumber, const unsigned int endPageNumber)
{
    if (endPageNumber < startPageNumber)
    {
        return IAP_INVALID_SECTOR;
    }

    const unsigned int startSector = startPageNumber / PagesPerSector;
    const unsigned int endSector = endPageNumber / PagesPerSector;

    for (unsigned int sector = startSector; sector <= endSector; ++sector)
    {
        const uint32_t sectorOffset = sector * FLASH_SECTOR_SIZE;
        if (!isStorageOffsetRange(sectorOffset, FLASH_SECTOR_SIZE))
        {
            return IAP_INVALID_SECTOR;
        }

        const uint8_t* sectorFlash = iapAddressOfSector(sector);
        memcpy(sectorBuffer, sectorFlash, FLASH_SECTOR_SIZE);

        const unsigned int firstPageInSector = sector * PagesPerSector;
        unsigned int eraseStartPage = startPageNumber;
        if (eraseStartPage < firstPageInSector)
        {
            eraseStartPage = firstPageInSector;
        }

        unsigned int eraseEndPage = endPageNumber;
        const unsigned int lastPageInSector = firstPageInSector + PagesPerSector - 1u;
        if (eraseEndPage > lastPageInSector)
        {
            eraseEndPage = lastPageInSector;
        }

        for (unsigned int page = eraseStartPage; page <= eraseEndPage; ++page)
        {
            const unsigned int pageIndex = page - firstPageInSector;
            memset(sectorBuffer + pageIndex * FLASH_PAGE_SIZE, 0xff, FLASH_PAGE_SIZE);
        }

        IAP_Status rc = eraseOffsetRange(sectorOffset, FLASH_SECTOR_SIZE);
        if (rc != IAP_SUCCESS)
        {
            return rc;
        }

        if (!bufferIsErased(sectorBuffer, FLASH_SECTOR_SIZE))
        {
            rc = programOffsetRange(sectorOffset, sectorBuffer, FLASH_SECTOR_SIZE);
            if (rc != IAP_SUCCESS)
            {
                return rc;
            }
        }
    }

    return IAP_SUCCESS;
}

IAP_Status iapProgram(uint8_t* rom, const uint8_t* ram, unsigned int size)
{
    if (!isStorageAddressRange(rom, size))
    {
        return IAP_DST_ADDR_NOT_MAPPED;
    }

    uint32_t offset = 0;
    if (!addressToFlashOffset(rom, offset))
    {
        return IAP_DST_ADDR_NOT_MAPPED;
    }

    return programOffsetRange(offset, ram, size);
}

IAP_Status iapReadUID(byte* uid)
{
    if (!uid)
    {
        return IAP_SRC_ADDR_ERROR;
    }

    pico_unique_board_id_t boardId;
    pico_get_unique_board_id(&boardId);

    memset(uid, 0, IAP_UID_LENGTH);
    const size_t copySize = PICO_UNIQUE_BOARD_ID_SIZE_BYTES < IAP_UID_LENGTH ?
            PICO_UNIQUE_BOARD_ID_SIZE_BYTES : IAP_UID_LENGTH;
    memcpy(uid, boardId.id, copySize);

    return IAP_SUCCESS;
}

IAP_Status iapReadPartID(unsigned int* partId)
{
    if (!partId)
    {
        return IAP_SRC_ADDR_ERROR;
    }

    *partId = 0;
    return IAP_INVALID_COMMAND;
}

unsigned int iapSectorOfAddress(const uint8_t * address)
{
    return (unsigned int)((address - FLASH_BASE_ADDRESS) / FLASH_SECTOR_SIZE);
}

unsigned int iapPageOfAddress(const uint8_t * address)
{
    return (unsigned int)((address - FLASH_BASE_ADDRESS) / FLASH_PAGE_SIZE);
}

uint8_t * iapAddressOfPage(const unsigned int page)
{
    return (page * FLASH_PAGE_SIZE) + FLASH_BASE_ADDRESS;
}

uint8_t * iapAddressOfSector(const unsigned int sector)
{
    return (sector * FLASH_SECTOR_SIZE) + FLASH_BASE_ADDRESS;
}

unsigned int iapFlashSize()
{
    if (iapFlashBytes)
    {
        return iapFlashBytes;
    }

#ifdef PICO_FLASH_SIZE_BYTES
    iapFlashBytes = PICO_FLASH_SIZE_BYTES;
#else
#error "PICO_FLASH_SIZE_BYTES is not defined for this board/target."
#endif

    return iapFlashBytes;
}

#else

// The maximum memory that is tested when searching for the flash size, in bytes
#define MAX_FLASH_SIZE 0x80000 // (524kB)

// The increments when searching for the flash size
#define FLASH_SIZE_SEARCH_INC 0x2000


/**
 * IAP command codes.
 */
enum IAP_Command
{
    CMD_PREPARE = 50,           //!< Prepare sector(s) for write
    CMD_COPY_RAM2FLASH = 51,    //!< Copy RAM to Flash
    CMD_ERASE = 52,             //!< Erase sector(s)
    CMD_BLANK_CHECK = 53,       //!< Blank check sector(s)
    CMD_READ_PART_ID = 54,      //!< Read chip part ID
    CMD_READ_BOOT_VER = 55,     //!< Read chip boot code version
    CMD_COMPARE = 56,           //!< Compare memory areas
    CMD_REINVOKE_ISP = 57,      //!< Reinvoke ISP
    CMD_READ_UID = 58,          //!< Read unique ID
    CMD_ERASE_PAGE = 59         //!< Erase page(s)
};

/**
 * A container for the interface to the IAP function calls.
 */
struct IAP_Parameter
{
    uintptr_t cmd;         //!< Command
    uintptr_t par[4];      //!< Parameters
    uintptr_t stat;        //!< Status
    uintptr_t res[4];      //!< Result
};

// The size of the flash in bytes. Use iapFlashSize() to get the flash size.
unsigned int iapFlashBytes = 0;


/** 
 * IAP call function (DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING!)
 * use instead: IAP_Call_InterruptSafe()
 */
typedef void (*IAP_Func)(uintptr_t * cmd, uintptr_t * stat);

#ifndef IAP_EMULATION
#  if defined(__LPC11XX__) || defined(__LPC11UXX__) || defined(__LPC13XX__) || defined(__LPC17XX__)
#    define IAP_LOCATION      0x1FFF1FF1
#  elif defined(__LPC2XXX__)
#    define IAP_LOCATION      0x7FFFFFF1
#  else
#    error "Unsupported processor"
#  endif
#  define IAP_Call ((IAP_Func) IAP_LOCATION)
#else
   extern "C" void IAP_Call (uintptr_t * cmd, uintptr_t * stat);
#endif


/**
 * IAP_Call_InterruptSafe(): interrupt-safe IAP_Call function
 *
 * ATTENTION: interrupts shall be blocked during an IAP_Call()!
 *
 * Reason: during an IAP_Call() with flash access the flash is inaccessible for
 *         the user application. When an interrupt occurs and the Interrupt
 *         Vector Table is located in the Flash this will fail and raise a
 *         non-handled HardFault condition.
 */
inline void IAP_Call_InterruptSafe(uintptr_t * cmd, uintptr_t * stat, const bool getLock = true)
{
    if (getLock)
    {
        noInterrupts();
    }

    IAP_Call(cmd, stat);

    if (getLock)
    {
        interrupts();
    }
}

static IAP_Status _prepareSectorRange(const unsigned int startSector, const unsigned int endSector, const bool getLock = true)
{
    IAP_Parameter p;

    p.cmd = CMD_PREPARE;
    p.par[0] = startSector;
    p.par[1] = endSector;
    IAP_Call_InterruptSafe(&p.cmd, &p.stat, getLock);

    return (IAP_Status) p.stat;
}

IAP_Status iapEraseSector(const unsigned int sector)
{
    return iapEraseSectorRange(sector, sector);
}

IAP_Status iapEraseSectorRange(const unsigned int startSector, const unsigned int endSector)
{
    IAP_Parameter p;

    p.stat = _prepareSectorRange(startSector, endSector);

    if (p.stat == IAP_SUCCESS)
    {
        p.cmd = CMD_ERASE;
        p.par[0] = startSector;
        p.par[1] = endSector;
        p.par[2] = SystemCoreClock / 1000;
        IAP_Call_InterruptSafe(&p.cmd, &p.stat);

        if (p.stat == IAP_SUCCESS)
        {
            p.cmd = CMD_BLANK_CHECK;
            p.par[0] = startSector;
            p.par[1] = endSector;
            IAP_Call_InterruptSafe(&p.cmd, &p.stat);
        }
    }
    return (IAP_Status) p.stat;
}

IAP_Status iapErasePage(const unsigned int pageNumber)
{
    return iapErasePageRange(pageNumber, pageNumber);
}

IAP_Status iapErasePageRange(const unsigned int startPageNumber, const unsigned int endPageNumber)
{
    unsigned int startSector = startPageNumber / (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE); // each sector has 16 pages
    unsigned int endSector = endPageNumber / (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE); // each sector has 16 pages
    IAP_Parameter p;

    p.stat = _prepareSectorRange(startSector, endSector); // even not mentioned in manual, this prepare is needed

    if (p.stat == IAP_SUCCESS)
    {
        p.cmd = CMD_ERASE_PAGE;
        p.par[0] = startPageNumber;
        p.par[1] = endPageNumber;
        p.par[2] = SystemCoreClock / 1000;
        IAP_Call_InterruptSafe(&p.cmd, &p.stat);
    }
    return (IAP_Status) p.stat;
}

IAP_Status iapProgram(uint8_t * rom, const uint8_t * ram, unsigned int size)
{
    // IMPORTANT: Address of ram must be word aligned. Otherwise you'll run into a IAP_SRC_ADDR_ERROR
    // Use '__attribute__ ((aligned (FLASH_PAGE_ALIGNMENT)))' to force correct alignment even with compiler optimization -Ox

    IAP_Parameter p;
    uint32_t startSector = iapSectorOfAddress(rom);
    uint32_t endSector = iapSectorOfAddress(rom + size - 1);

    // in order to access flash we need to disable all interrupts
    noInterrupts();
    // first we need to unlock the sector
    p.stat = _prepareSectorRange(startSector, endSector, false);

    if (p.stat != IAP_SUCCESS)
    {
        interrupts();
        return (IAP_Status) p.stat;
    }

    // then we can copy the RAM content to the FLASH
    p.cmd = CMD_COPY_RAM2FLASH;
    p.par[0] = (uintptr_t) rom;
    p.par[1] = (uintptr_t) ram;
    p.par[2] = size;
    p.par[3] = SystemCoreClock / 1000;
    IAP_Call_InterruptSafe(&p.cmd, &p.stat, false);

    if (p.stat != IAP_SUCCESS)
    {
        interrupts();
        return (IAP_Status) p.stat;
    }

    // now we check that RAM and FLASH have the same content
    p.cmd = CMD_COMPARE;
    p.par[0] = (uintptr_t) rom;
    p.par[1] = (uintptr_t) ram;
    p.par[2] = size;
    IAP_Call_InterruptSafe(&p.cmd, &p.stat, false);
    interrupts();
    return (IAP_Status) p.stat;
}

IAP_Status iapReadUID(byte* uid)
{
    IAP_Parameter p;
    p.cmd = CMD_READ_UID;

    IAP_Call_InterruptSafe(&p.cmd, &p.stat);
    memcpy(uid, p.res, 16);

    return (IAP_Status) p.stat;
}

IAP_Status iapReadPartID(unsigned int* partId)
{
    IAP_Parameter p;
    p.cmd = CMD_READ_PART_ID;

    IAP_Call_InterruptSafe(&p.cmd, &p.stat);
    *partId = p.res[0];

    return (IAP_Status) p.stat;
}

unsigned int iapSectorOfAddress(const uint8_t * address)
{
    return (unsigned int)((address - FLASH_BASE_ADDRESS) / FLASH_SECTOR_SIZE);
}

unsigned int iapPageOfAddress(const uint8_t * address)
{
    return (unsigned int)((address - FLASH_BASE_ADDRESS) / FLASH_PAGE_SIZE);
}

uint8_t * iapAddressOfPage(const unsigned int page)
{
    return (page * FLASH_PAGE_SIZE) + FLASH_BASE_ADDRESS;
}

uint8_t * iapAddressOfSector(const unsigned int sector)
{
    return (sector * FLASH_SECTOR_SIZE) + FLASH_BASE_ADDRESS;
}

unsigned int iapFlashSize()
{
    if (iapFlashBytes)
        return iapFlashBytes;

    IAP_Parameter p;
    p.cmd = CMD_BLANK_CHECK;

    const int sectorInc = FLASH_SIZE_SEARCH_INC / FLASH_SECTOR_SIZE;
    unsigned int sector = sectorInc;
    const int maxSector = MAX_FLASH_SIZE / FLASH_SECTOR_SIZE;

    while (sector < maxSector)
    {
        p.par[0] = sector;
        p.par[1] = sector;
        IAP_Call_InterruptSafe(&p.cmd, &p.stat);

        if (p.stat == IAP_INVALID_SECTOR)
            break;

        sector += sectorInc;
    }

    iapFlashBytes = sector * FLASH_SECTOR_SIZE;
    return iapFlashBytes;
}

#endif