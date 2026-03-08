/*
 * platform.h - Low level hardware specific stuff.
 *
 * Copyright (c) 2014 Stefan Taferner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#ifndef sblib_platform_h
#define sblib_platform_h

// Per Herb Sutter, the register keyword did not have any effect on C++ programs at all:
//
// https://www.drdobbs.com/keywords-that-arent-or-comments-by-anoth/184403859
//
// That was back in 2003, but ARM still used the keyword in CMSIS.
// Nowadays, with C++17 using register yields the following warning:
//
// warning: ISO C++17 does not allow 'register' storage class specifier [-Wregister]
//
// As compiler output is indeed exactly the same, regardless of whether register is
// specified or not, it's ok to hide it from the compiler with the preprocessor's help.
#define register

#if defined(__LPC11XX__)

#include <LPC11xx.h>
#include <core_cm0.h>

/**
 * Low level table of the IO ports
 */
extern LPC_GPIO_TypeDef* const gpioPorts[4];

/**
 * Get a pointer to a low level IO configuration register.
 *
 * @param pin - the IO pin to get the configuration for
 * @return a pointer to the IO configuration register.
 */
unsigned int* ioconPointer(int pin);

/**
 * Get a pointer to a low level IO configuration register.
 *
 * @param port - the IO port to get the configuration for
 * @param pinNum - the number of the pin to get the configuration for
 * @return a pointer to the IO configuration register.
 */
unsigned int* ioconPointer(int port, int pinNum);

#elif defined(SBLIB_PLATFORM_RP2354) || defined(PICO_RP2350)

#include <stdint.h>
#include "pico.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

/**
 * Translate an sblib PortPin encoding to a RP2354 GPIO number.
 *
 * RP2354 has no LPC-style GPIO ports. For compatibility we encode
 * GPIO0..31  as PIO0_0..PIO0_31
 * GPIO32..47 as PIO1_0..PIO1_15
 */
ALWAYS_INLINE static unsigned int sblibGpioNumber(int pin)
{
    return (((unsigned int) pin >> 5) & 0x3u) * 32u + ((unsigned int) pin & 31u);
}

/**
 * Return true if the PortPin encoding maps to a valid RP2354 GPIO.
 */
ALWAYS_INLINE static bool sblibValidGpio(int pin)
{
    return sblibGpioNumber(pin) < 48u;
}

#else
#error "Unsupported platform"
#endif

#ifdef IAP_EMULATION
extern uint8_t FLASH[];
# define LPC_FLASH_BASE (FLASH)
#else
#ifndef LPC_FLASH_BASE
#define LPC_FLASH_BASE 0
#endif
#endif

#define FLASH_BASE_ADDRESS ((uint8_t *)LPC_FLASH_BASE) //!< The base address of the flash
#define FLASH_SECTOR_SIZE (0x1000) //!< The size of a flash sector in bytes
#define FLASH_PAGE_SIZE (0x100) //!< The size of a flash page in bytes
#define FLASH_PAGE_ALIGNMENT (FLASH_PAGE_SIZE - 1) //!< Page alignment which is allowed to flash
#define FLASH_RAM_BUFFER_ALIGNMENT (4) //!< MCU's RAM buffer alignment which is allowed to flash

#endif /*sblib_platform_h*/