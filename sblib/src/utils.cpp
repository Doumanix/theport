/*
 * utils.cpp - Misc utility functions.
 *
 * RP2354 port:
 * - no LPC SysTick assumptions
 * - reverseCopy() implemented
 * - KNX TX pin API kept consistent with utils.h
 */

#include "sblib/utils.h"
#include "sblib/digital_pin.h"
#include "sblib/interrupt.h"

#if defined(SBLIB_PLATFORM_RP2354) || defined(PICO_RP2350)
#include "hardware/gpio.h"
#endif

namespace
{
#if defined(SBLIB_PLATFORM_RP2354) || defined(PICO_RP2350)
    static int fatalErrorPin = -1;
    static int knxTxPin = -1;
#else
    static int fatalErrorPin = PIN_PROG;
    static int knxTxPin = PIN_EIB_TX; ///\todo make it universal
#endif
}

void reverseCopy(byte* dest, const byte* src, int len)
{
    if (!dest || !src || len <= 0)
        return;

    for (int i = 0; i < len; ++i)
        dest[i] = src[len - 1 - i];
}

void setFatalErrorPin(int pin)
{
    fatalErrorPin = pin;
}

void setKNX_TX_Pin(int pin)
{
    knxTxPin = pin;
}

int getFatalErrorPin()
{
    return fatalErrorPin;
}

int getKNX_TX_Pin()
{
    return knxTxPin;
}

int hashUID(byte* uid, const int len_uid, byte* hash, const int len_hash)
{
    if (!uid || !hash || len_uid <= 0 || len_hash <= 0)
        return 0;

    // Simple deterministic compatibility hash.
    // Good enough as a portable fallback until/if the original LPC-specific
    // implementation is needed bit-identically.
    uint32_t h = 2166136261u; // FNV-1a basis

    for (int i = 0; i < len_uid; ++i)
    {
        h ^= uid[i];
        h *= 16777619u;
    }

    for (int i = 0; i < len_hash; ++i)
    {
        h ^= (h >> 13);
        h *= 16777619u;
        hash[i] = static_cast<byte>((h >> 24) ^ (h >> 16) ^ (h >> 8) ^ h);
    }

    return 1;
}

#if defined(SBLIB_PLATFORM_RP2354) || defined(PICO_RP2350)

static void fatal_blink_loop(unsigned int period_mask)
{
    unsigned int ctr = 0;

    if (fatalErrorPin >= 0)
    {
        pinMode(fatalErrorPin, OUTPUT);
        digitalWrite(fatalErrorPin, 0);
    }

    if (knxTxPin >= 0)
    {
        pinMode(knxTxPin, OUTPUT);
        digitalWrite(knxTxPin, 1);
    }

    noInterrupts();

    while (true)
    {
        ++ctr;

        if (fatalErrorPin >= 0)
            digitalWrite(fatalErrorPin, (ctr & period_mask) == 0 ? 1 : 0);

        if (knxTxPin >= 0)
            digitalWrite(knxTxPin, (ctr & period_mask) == 0 ? 0 : 1);
    }
}

void fatalError()
{
    fatal_blink_loop(0x40000u);
}

extern "C" void HardFault_Handler()
{
    fatal_blink_loop(0x20000u);
}

#else

void fatalError()
{
    pinMode(fatalErrorPin, OUTPUT);
    pinMode(knxTxPin, OUTPUT);

    SysTick_Config(0x1000000);
    noInterrupts();

    while (true)
    {
        digitalWrite(fatalErrorPin, (SysTick->VAL & 0x400000) == 0 ? 1 : 0);
        digitalWrite(knxTxPin, (SysTick->VAL & 0x400000) == 0 ? 0 : 1);
    }
}

extern "C" void HardFault_Handler()
{
    pinMode(fatalErrorPin, OUTPUT);
    pinMode(knxTxPin, OUTPUT);

    SysTick_Config(0x1000000);
    noInterrupts();

    while (true)
    {
        digitalWrite(fatalErrorPin, (SysTick->VAL & 0x200000) == 0 ? 1 : 0);
        digitalWrite(knxTxPin, (SysTick->VAL & 0x200000) == 0 ? 0 : 1);
    }
}

#endif