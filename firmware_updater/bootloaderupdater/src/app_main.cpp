#include <sblib/internal/iap.h>
#include <sblib/io_pin_names.h>
#include <sblib/digital_pin.h>
#include <sblib/version.h>
#include <cstdint>
#include <cstring>

#ifdef DEBUG
#   include <sblib/serial.h>
#endif

#ifdef DEBUG
#   define d(x) {x;}
#else
#   define d(x)
#endif

// Remember to change build-variable sw_version in the .cproject file
constexpr uint8_t BOOTLOADERUPDATER_MAJOR_VERSION = 1;  //!< BootloaderUpdater major version @note change also in @ref APP_VERSION
constexpr uint8_t BOOTLOADERUPDATER_MINOR_VERSION = 20; //!< BootloaderUpdater minor Version @note change also in @ref APP_VERSION

// changes of the app version string must also be done in BootloaderUpdater.java of the Selfbus-Updater
APP_VERSION("SBblu   ", "1", "20");

extern const __attribute__((aligned(16))) uint8_t incbin_bl_start[];
extern const uint8_t incbin_bl_end[];

///\todo Create common header of constants shared between BL and BLU
#define BOOTLOADER_FLASH_STARTADDRESS ((uint8_t *) 0x0) //!< Flash start address of the bootloader

/** Size of the BootDescriptorBlock. Must match the BOOT_BLOCK_DESC_SIZE of the BL in the boot_descriptor_block.h */
constexpr uint16_t BOOT_BLOCK_DESC_SIZE = 0x100; // same as FLASH_PAGE_SIZE of sblib/platform.h

void setup()
{
    pinMode(PIN_PROG, OUTPUT);
    digitalWrite(PIN_PROG, false);

    d(
        //serial.setRxPin(PIO3_1);
        //serial.setTxPin(PIO3_0);
        if (!serial.enabled())
        {
            serial.begin(115200);
        }
        serial.println();
        serial.print("Selfbus BootloaderUpdater v", BOOTLOADERUPDATER_MAJOR_VERSION);
        serial.print(".", BOOTLOADERUPDATER_MINOR_VERSION);
        serial.println(" DEBUG MODE :-)");
        serial.print("Build: ");
        serial.print(__DATE__);
        serial.print(" ");
        serial.println(__TIME__);
        serial.flush();
    );
}

void SystemReset()
{
    d(
        serial.println("RESET");
        serial.flush();
    )
    NVIC_SystemReset();
}

int main()
{
    setup();

    const unsigned int newBlSize = (incbin_bl_end - incbin_bl_start);
    const uint8_t* newBlEndAddress = BOOTLOADER_FLASH_STARTADDRESS + newBlSize - 1;
    const unsigned int newBlStartSector = iapSectorOfAddress(BOOTLOADER_FLASH_STARTADDRESS);
    const unsigned int newBlEndSector = iapSectorOfAddress(newBlEndAddress);

    d(
        serial.println("newBlSize: 0x", newBlSize, HEX, 4);
        serial.print("Erasing Sectors: ", newBlStartSector);
        serial.print(" - ", newBlEndSector);
    )

    if (iapEraseSectorRange(newBlStartSector, newBlEndSector) != IAP_SUCCESS)
    {
        d(serial.println(" --> FAILED");)
        SystemReset();
    }

    d(serial.println(" --> done");)

    for (const uint8_t * i = incbin_bl_start; i < incbin_bl_end; i += FLASH_SECTOR_SIZE)
    {
        __attribute__ ((aligned (FLASH_RAM_BUFFER_ALIGNMENT))) byte buf[FLASH_SECTOR_SIZE]; // Address of buf must be word aligned, see iapProgram(..) hint.
        memset(buf, 0xFF, FLASH_SECTOR_SIZE);
        unsigned int len = incbin_bl_end - i;
        if (len > FLASH_SECTOR_SIZE)
        {
            len = FLASH_SECTOR_SIZE;
        }
        memcpy(buf, i, len);

        uint8_t * flash = BOOTLOADER_FLASH_STARTADDRESS + (i - incbin_bl_start);

        if (flash == nullptr)
        {
            // NXP bootloader uses an Int-Vect as a checksum to see if the application is valid.
            // If the value is not correct, then it does not start the application
            // Vector table starts always at base address. Each entry is 4 bytes.
            uint32_t checksum = 0;
            for (int j = 0; j < 7; j++) // Checksum is 2's complement of entries 0 through 6
            {
                checksum += *(int*)&buf[j*4];
            }
            checksum = -checksum;
            *(int*)&buf[28] = checksum;
            d(serial.println("checksum: 0x", (int) checksum, HEX);)
        }
        d(serial.print("flashing 0x", flash);)
        if (iapProgram(flash, buf, FLASH_SECTOR_SIZE) != IAP_SUCCESS)
        {
            d(serial.println(" --> FAILED");)
            SystemReset();
        }
        d(serial.println(" --> done");)
        digitalWrite(PIN_PROG, !digitalRead(PIN_PROG));
    }

    // Make sure that the current boot descriptor of the BLU is erased,
    // otherwise the BL will restart the BLU in an endless loop.
    const uint32_t bootDescriptorBlockPage = iapPageOfAddress(newBlEndAddress + BOOT_BLOCK_DESC_SIZE);
    d(serial.println("Erasing BootDescriptorPage: 0x", (unsigned int)bootDescriptorBlockPage, HEX);)
    if (iapErasePageRange(bootDescriptorBlockPage, bootDescriptorBlockPage) != IAP_SUCCESS)
    {
        d(serial.println(" --> FAILED");)
    }
    else
    {
        d(serial.println(" --> done");)
    }

    SystemReset();
}
