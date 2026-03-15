/*
 *  timer.h - Timer manipulation and time functions.
 *
 *  Copyright (c) 2014 Stefan Taferner <stefan.taferner@gmx.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */
#ifndef sblib_timer_h
#define sblib_timer_h

#include <limits.h>
#include <sblib/platform.h>
#include <sblib/types.h>

class Timer;

/**
 * Delay the program execution by sleeping a number of milliseconds.
 * SysTick Interrupt must be enabled.
 *
 * @param msec - the number of milliseconds to sleep.
 */
void delay(unsigned int msec);

/**
 * The number of minimal Microseconds possible for delayMicroseconds().
 */
#define MIN_DELAY_MICROSECONDS 2

/**
 * The number of maximal Microseconds possible for delay by delayMicroseconds().
 */
#define MAX_DELAY_MICROSECONDS (4000)

/**
 * The number of maximal Milliseconds possible for delay by delayMicroseconds().
 */
#define MAX_DELAY_MILLISECONDS (MAX_DELAY_MICROSECONDS / 1000)

/**
 * Delay the program execution a number of microseconds.
 *
 * @param[in] usec - the number of microseconds to wait.
 */
void delayMicroseconds(unsigned int usec);

#ifdef IAP_EMULATION
void setMillis(unsigned int newSystemTime);
#endif

unsigned int millis();
unsigned int elapsed(unsigned int ref);

/**
 * The number of CPU clock cycles per microsecond.
 */
#if defined(SBLIB_PLATFORM_RP2354) || defined(PICO_RP2350)
#define clockCyclesPerMicrosecond() (F_CPU / 1000000UL)
#else
#define clockCyclesPerMicrosecond() (SystemCoreClock / 1000000)
#endif

#define clockCyclesToMicroseconds(cyc) ((cyc) / clockCyclesPerMicrosecond())
#define microsecondsToClockCycles(msec) ((msec) * clockCyclesPerMicrosecond())

#ifndef F_CPU
#   define F_CPU (150000000UL)
#endif

#define DELAY_USEC_HIGH_PRECISION(usec) delay_cycles((double)F_CPU * (usec) / 1E6 / 6.0+1)

extern Timer timer16_0;
extern Timer timer16_1;
extern Timer timer32_0;
extern Timer timer32_1;

/**
 * A timer.
 */
class Timer
{
public:
    Timer(byte timerNum);

    void begin();
    void end();

    void prescaler(unsigned int factor);
    unsigned int prescaler() const;

    void start();
    void stop();
    void restart();
    void reset();

    unsigned int value() const;
    void value(unsigned int val);

    void interrupts();
    void noInterrupts();

    int flags() const;
    void resetFlags();
    void resetFlag(TimerMatch match);
    void resetFlag(TimerCapture capture);
    bool flag(TimerMatch match) const;
    bool flag(TimerCapture cap) const;
    static int flagMask(TimerMatch match);
    static int flagMask(TimerCapture cap);

    void matchMode(int channel, int mode);
    int matchMode(int channel) const;

    void match(int channel, unsigned int value);
    unsigned int match(int channel) const;

    void captureMode(int channel, int mode);
    int captureMode(int channel) const;

    unsigned int capture(int channel) const;

    void pwmEnable(int channel);
    void pwmDisable(int channel);

    void counterMode(int mode, int clearMode);
    void matchModePinConfig(int channel, int mode);

    bool is32bitTimer(void);
    bool getMatchChannelLevel(int channel);

    void setIRQPriority(uint32_t newPriority);

protected:
#if defined(__LPC11XX__)
    LPC_TMR_TypeDef* timer;
#endif
    byte timerNum;
};

#if defined(__LPC11XX__)

ALWAYS_INLINE void Timer::prescaler(unsigned int factor)
{
    timer->PR = factor;
}

ALWAYS_INLINE unsigned int Timer::prescaler() const
{
    return timer->PR;
}

ALWAYS_INLINE void Timer::start()
{
    timer->TCR |= 1;
}

ALWAYS_INLINE void Timer::stop()
{
    timer->TCR &= ~1;
}

ALWAYS_INLINE void Timer::end()
{
    LPC_SYSCON->SYSAHBCLKCTRL &= ~(1 << (7 + timerNum));
}

ALWAYS_INLINE void Timer::restart()
{
    timer->TCR = 2;
    timer->TCR = 1;
}

ALWAYS_INLINE void Timer::reset()
{
    timer->TCR |= 2;
    timer->TCR &= ~2;
}

ALWAYS_INLINE unsigned int Timer::value() const
{
    return timer->TC;
}

ALWAYS_INLINE void Timer::value(unsigned int val)
{
    timer->TC = val;
}

ALWAYS_INLINE int Timer::flags() const
{
    return timer->IR;
}

ALWAYS_INLINE void Timer::resetFlags()
{
    timer->IR = 0xff;
}

ALWAYS_INLINE void Timer::resetFlag(TimerMatch match)
{
    timer->IR = (1 << match);
}

ALWAYS_INLINE void Timer::resetFlag(TimerCapture capture)
{
    timer->IR = (16 << capture);
}

ALWAYS_INLINE bool Timer::flag(TimerMatch match) const
{
    return timer->IR & (1 << match);
}

ALWAYS_INLINE bool Timer::flag(TimerCapture capture) const
{
    return timer->IR & (16 << capture);
}

ALWAYS_INLINE int Timer::flagMask(TimerMatch match)
{
    return 1 << match;
}

ALWAYS_INLINE int Timer::flagMask(TimerCapture capture)
{
    return 16 << capture;
}

ALWAYS_INLINE void Timer::interrupts()
{
    NVIC_EnableIRQ((IRQn_Type) (TIMER_16_0_IRQn + timerNum));
}

ALWAYS_INLINE void Timer::noInterrupts()
{
    NVIC_DisableIRQ((IRQn_Type) (TIMER_16_0_IRQn + timerNum));
}

ALWAYS_INLINE unsigned int Timer::match(int channel) const
{
    return (&timer->MR0)[channel];
}

ALWAYS_INLINE void Timer::match(int channel, unsigned int value)
{
    (&timer->MR0)[channel] = value;
}

ALWAYS_INLINE unsigned int Timer::capture(int channel) const
{
    return (&timer->CR0)[channel];
}

ALWAYS_INLINE void Timer::pwmEnable(int channel)
{
    timer->PWMC |= 1 << channel;
}

ALWAYS_INLINE void Timer::pwmDisable(int channel)
{
    timer->PWMC &= ~(1 << channel);
}

ALWAYS_INLINE void Timer::matchModePinConfig(int channel, int mode)
{
    int offset = channel << 1;
    timer->EMR = (timer->EMR
               & ~(0x30 << offset))
               | ((mode & 0x30) << offset);
}

ALWAYS_INLINE bool Timer::is32bitTimer(void)
{
    return (bool)(this->timerNum > 1);
}

ALWAYS_INLINE bool Timer::getMatchChannelLevel(int channel)
{
    return (bool)(timer->EMR & (1 << channel));
}

#else

ALWAYS_INLINE int Timer::flagMask(TimerMatch match)
{
    return 1 << match;
}

ALWAYS_INLINE int Timer::flagMask(TimerCapture capture)
{
    return 16 << capture;
}

ALWAYS_INLINE bool Timer::is32bitTimer(void)
{
    return (bool)(this->timerNum > 1);
}

#endif

static inline void delay_cycles(unsigned int cycles) __attribute__((always_inline));

static inline void delay_cycles(unsigned int cycles)
{
    __asm__ volatile (".syntax unified" "\n\t"
          " .align 4\n"
          "1: subs %[cycles], %[cycles], #1" "\n\t"
          "cmp %[cycles], #0" "\n\t"
          "bne 1b"
          : [cycles] "=l" (cycles)
          : "0" (cycles)
          : "cc");
}

#endif /*sblib_timer_h*/