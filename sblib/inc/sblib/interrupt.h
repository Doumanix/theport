/*
 * interrupt.h - Interrupt related functions.
 *
 * RP2354 first-cut port:
 * - LPC11xx path keeps CMSIS/NVIC behavior
 * - RP2354 path avoids LPC/CMSIS type assumptions and uses Cortex-M NVIC
 *   registers plus inline assembly for global IRQ control
 */

#ifndef sblib_interrupt_h
#define sblib_interrupt_h

#include "sblib/platform.h"
#include "sblib/types.h"

#if defined(__LPC11XX__)

/**
 * Disable all interrupts.
 */
ALWAYS_INLINE void noInterrupts()
{
    __DSB();
    __ISB();
    __disable_irq();
}

/**
 * Enable all interrupts.
 */
ALWAYS_INLINE void interrupts()
{
    __enable_irq();
}

/**
 * Wait for interrupt.
 */
ALWAYS_INLINE void waitForInterrupt()
{
    __WFI();
}

ALWAYS_INLINE void enableInterrupt(IRQn_Type interruptType)
{
    NVIC_EnableIRQ(interruptType);
}

ALWAYS_INLINE void disableInterrupt(IRQn_Type interruptType)
{
    NVIC_DisableIRQ(interruptType);
}

ALWAYS_INLINE void clearPendingInterrupt(IRQn_Type interruptType)
{
    NVIC_ClearPendingIRQ(interruptType);
}

ALWAYS_INLINE void setPendingInterrupt(IRQn_Type interruptType)
{
    NVIC_SetPendingIRQ(interruptType);
}

ALWAYS_INLINE bool isInsideInterrupt()
{
    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0;
}

ALWAYS_INLINE bool getInterruptEnabled(IRQn_Type interruptType)
{
    return NVIC->ISER[((unsigned int) interruptType) >> 5] & (1u << (((unsigned int) interruptType) & 31u));
}

#elif defined(SBLIB_PLATFORM_RP2354) || defined(PICO_RP2350)

/*
 * RP2354 path
 *
 * We intentionally do not depend on CMSIS IRQn_Type here yet, because the
 * current RP2354 build path is being brought up incrementally and should not
 * require the LPC/CMSIS type universe to compile.
 */

ALWAYS_INLINE void noInterrupts()
{
    __asm volatile (
        "dsb 0xF\n"
        "isb 0xF\n"
        "cpsid i\n"
        :
        :
        : "memory"
    );
}

ALWAYS_INLINE void interrupts()
{
    __asm volatile (
        "cpsie i\n"
        :
        :
        : "memory"
    );
}

ALWAYS_INLINE void waitForInterrupt()
{
    __asm volatile (
        "wfi\n"
        :
        :
        : "memory"
    );
}

ALWAYS_INLINE void enableInterrupt(int interruptType)
{
    volatile unsigned int* iser =
        (volatile unsigned int*) (0xE000E100u + ((((unsigned int) interruptType) >> 5u) << 2u));
    *iser = 1u << (((unsigned int) interruptType) & 31u);
}

ALWAYS_INLINE void disableInterrupt(int interruptType)
{
    volatile unsigned int* icer =
        (volatile unsigned int*) (0xE000E180u + ((((unsigned int) interruptType) >> 5u) << 2u));
    *icer = 1u << (((unsigned int) interruptType) & 31u);
}

ALWAYS_INLINE void clearPendingInterrupt(int interruptType)
{
    volatile unsigned int* icpr =
        (volatile unsigned int*) (0xE000E280u + ((((unsigned int) interruptType) >> 5u) << 2u));
    *icpr = 1u << (((unsigned int) interruptType) & 31u);
}

ALWAYS_INLINE void setPendingInterrupt(int interruptType)
{
    volatile unsigned int* ispr =
        (volatile unsigned int*) (0xE000E200u + ((((unsigned int) interruptType) >> 5u) << 2u));
    *ispr = 1u << (((unsigned int) interruptType) & 31u);
}

ALWAYS_INLINE bool isInsideInterrupt()
{
    unsigned int ipsr;
    __asm volatile (
        "mrs %0, ipsr\n"
        : "=r" (ipsr)
        :
        : "memory"
    );
    return ipsr != 0;
}

ALWAYS_INLINE bool getInterruptEnabled(int interruptType)
{
    volatile unsigned int* iser =
        (volatile unsigned int*) (0xE000E100u + ((((unsigned int) interruptType) >> 5u) << 2u));
    return ((*iser) & (1u << (((unsigned int) interruptType) & 31u))) != 0;
}

#else
#error "Unsupported platform"
#endif

#endif /* sblib_interrupt_h */