/*
 *  timer.cpp - Timer manipulation and time functions.
 *
 *  Copyright (c) 2014 Stefan Taferner <stefan.taferner@gmx.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <sblib/timer.h>
#include <sblib/interrupt.h>
#include <sblib/utils.h>

#if defined(SBLIB_PLATFORM_RP2354) || defined(PICO_RP2350)

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "sb_gpio.h"

extern "C" void TIMER16_1_IRQHandler(void);

/*
 * RX/TX diagnostics for RP2354 bring-up.
 * These symbols can be declared as extern in main.cpp if needed:
 *
 * extern volatile unsigned int sb_rx_irq_count;
 * extern volatile unsigned int sb_rx_capture_count;
 * extern volatile unsigned int sb_rx_reject_short_count;
 * extern volatile unsigned int sb_rx_last_capture;
 * extern volatile unsigned int sb_rx_last_delta;
 * extern volatile unsigned int sb_tx_pwm_fire_count;
 * extern volatile unsigned int sb_tx_time_fire_count;
 */
volatile unsigned int sb_rx_irq_count = 0;
volatile unsigned int sb_rx_capture_count = 0;
volatile unsigned int sb_rx_reject_short_count = 0;
volatile unsigned int sb_rx_last_capture = 0;
volatile unsigned int sb_rx_last_delta = 0;
volatile unsigned int sb_tx_pwm_fire_count = 0;
volatile unsigned int sb_tx_time_fire_count = 0;

namespace
{
    struct RpTimerState
    {
        bool initialized = false;
        bool running = false;
        bool irqEnabled = false;

        unsigned int prescaler = 0;
        uint64_t startUs = 0;

        unsigned int lastAcceptedCapture = 0;
        bool hasAcceptedCapture = false;

        unsigned int matchValue[4] = {0xffffu, 0xffffu, 0xffffu, 0xffffu};
        int matchModeValue[4] = {0, 0, 0, 0};

        unsigned int captureValueArr[2] = {0u, 0u};
        int captureModeValue[2] = {0, 0};

        bool pwmEnabled[4] = {false, false, false, false};
        bool matchLevel[4] = {false, false, false, false};

        volatile uint32_t flags = 0;
        alarm_id_t alarmId = 0;
    };

    RpTimerState gTimers[4];

    inline uint32_t timer_mask(byte timerNum)
    {
        return (timerNum > 1) ? 0xffffffffu : 0xffffu;
    }

    inline uint64_t now_us()
    {
        return time_us_64();
    }

    inline RpTimerState& state_for(byte timerNum)
    {
        return gTimers[timerNum];
    }

    inline unsigned int current_value(byte timerNum)
    {
        auto& st = state_for(timerNum);
        if (!st.running)
            return 0;

        return (unsigned int)((now_us() - st.startUs) & timer_mask(timerNum));
    }

    void call_irq_handler(byte timerNum)
    {
        if (timerNum == TIMER16_1)
            TIMER16_1_IRQHandler();
    }

    // Für GPIO15 funktioniert die Sendestufe so:
    // idle = LOW
    // Bit   = HIGH pulse
    void drive_tx_idle_high()
    {
        gpio_put(PIN_EIB_TX, 0);
    }

    void drive_tx_pulse_low()
    {
        gpio_put(PIN_EIB_TX, 1);
    }

    void schedule_next_alarm(byte timerNum);

    int64_t timer_alarm_callback(alarm_id_t, void* userData)
    {
        byte timerNum = (byte)(uintptr_t)userData;
        auto& st = state_for(timerNum);
        if (!st.running)
            return 0;

        const unsigned int tv = current_value(timerNum);

        bool hadReset = false;
        bool firedPwmChannel = false;
        bool firedTimeChannel = false;

        // MAT0 / PWM channel: trigger independently of matchMode flags
        if (st.pwmEnabled[MAT0] && tv >= st.matchValue[MAT0])
        {
            firedPwmChannel = true;
        }

        // Normal match channels: respect matchMode flags
        for (int ch = 0; ch < 4; ++ch)
        {
            if (!(st.matchModeValue[ch] & (INTERRUPT | RESET | STOP | CLEAR | SET | TOGGLE)))
                continue;

            if (tv < st.matchValue[ch])
                continue;

            st.flags |= (1u << ch);

            if (ch == MAT2)
                firedTimeChannel = true;

            if (st.matchModeValue[ch] & RESET)
                hadReset = true;

            if (st.matchModeValue[ch] & STOP)
                st.running = false;
        }

        if (firedPwmChannel)
        {
            ++sb_tx_pwm_fire_count;
            drive_tx_pulse_low();
            st.matchLevel[MAT0] = true;
        }

        if (firedTimeChannel)
        {
            ++sb_tx_time_fire_count;
            drive_tx_idle_high();
            st.matchLevel[MAT0] = false;
        }

        if (hadReset)
            st.startUs = now_us();

        if (st.irqEnabled && st.flags)
            call_irq_handler(timerNum);

        schedule_next_alarm(timerNum);
        return 0;
    }

    void schedule_next_alarm(byte timerNum)
    {
        auto& st = state_for(timerNum);

        if (st.alarmId > 0)
        {
            cancel_alarm(st.alarmId);
            st.alarmId = 0;
        }

        if (!st.running)
            return;

        unsigned int tv = current_value(timerNum);
        unsigned int bestDelta = 0xffffffffu;
        bool found = false;

        // Consider PWM channel MAT0 as an event source too
        if (st.pwmEnabled[MAT0])
        {
            unsigned int mv = st.matchValue[MAT0];
            if (mv <= tv)
                mv = tv + 1;

            unsigned int delta = mv - tv;
            found = true;
            bestDelta = delta;
        }

        for (int ch = 0; ch < 4; ++ch)
        {
            if (!(st.matchModeValue[ch] & (INTERRUPT | RESET | STOP | CLEAR | SET | TOGGLE)))
                continue;

            unsigned int mv = st.matchValue[ch];
            if (mv <= tv)
                mv = tv + 1;

            unsigned int delta = mv - tv;
            if (!found || delta < bestDelta)
            {
                found = true;
                bestDelta = delta;
            }
        }

        if (!found)
            return;

        if (bestDelta == 0)
            bestDelta = 1;

        st.alarmId = add_alarm_in_us((int64_t)bestDelta, timer_alarm_callback, (void*)(uintptr_t)timerNum, true);
    }

    void gpio_capture_callback(uint gpio, uint32_t events)
    {
        if (gpio != PIN_EIB_RX)
            return;

        if ((events & GPIO_IRQ_EDGE_FALL) == 0)
            return;

        ++sb_rx_irq_count;

        gpio_acknowledge_irq(PIN_EIB_RX, GPIO_IRQ_EDGE_FALL);

        auto& st = state_for(TIMER16_1);
        if (!st.running || !st.irqEnabled)
            return;

        const unsigned int captureNow = current_value(TIMER16_1);
        sb_rx_last_capture = captureNow;

        if (st.hasAcceptedCapture)
        {
            const unsigned int delta = captureNow - st.lastAcceptedCapture;
            sb_rx_last_delta = delta;

            // vorher: delta < 40
            if (delta < 8)
            {
                ++sb_rx_reject_short_count;
                return;
            }
        }
        else
        {
            sb_rx_last_delta = 0;
        }

        st.lastAcceptedCapture = captureNow;
        st.hasAcceptedCapture = true;

        ++sb_rx_capture_count;

        st.captureValueArr[CAP0] = captureNow;
        st.flags |= (16u << CAP0);

        call_irq_handler(TIMER16_1);
    }
}

static volatile unsigned int systemTimeOffset = 0;

void delay(unsigned int msec)
{
    sleep_ms(msec);
}

#ifndef IAP_EMULATION
void delayMicroseconds(unsigned int usec)
{
    if (usec > MAX_DELAY_MICROSECONDS)
    {
        fatalError();
    }

    sleep_us(usec);
}
#endif

#ifdef IAP_EMULATION
void setMillis(unsigned int newSystemTime)
{
    unsigned int now = to_ms_since_boot(get_absolute_time());
    systemTimeOffset = newSystemTime - now;
}
#endif

unsigned int millis()
{
    return (unsigned int)to_ms_since_boot(get_absolute_time()) + systemTimeOffset;
}

unsigned int elapsed(unsigned int ref)
{
    return millis() - ref;
}

Timer timer16_0(TIMER16_0);
Timer timer16_1(TIMER16_1);
Timer timer32_0(TIMER32_0);
Timer timer32_1(TIMER32_1);

Timer::Timer(byte aTimerNum)
{
    timerNum = aTimerNum;
}

void Timer::begin()
{
    auto& st = state_for(timerNum);
    if (st.initialized)
        return;

    st.initialized = true;
    st.prescaler = 0;
    st.startUs = now_us();

    if (timerNum == TIMER16_1)
    {
        gpio_init(PIN_EIB_TX);
        gpio_set_dir(PIN_EIB_TX, GPIO_OUT);
        drive_tx_idle_high();

        gpio_init(PIN_EIB_RX);
        gpio_set_dir(PIN_EIB_RX, GPIO_IN);
        gpio_pull_up(PIN_EIB_RX);

        gpio_set_irq_enabled_with_callback(PIN_EIB_RX, GPIO_IRQ_EDGE_FALL, false, &gpio_capture_callback);
    }
}

void Timer::end()
{
    auto& st = state_for(timerNum);
    st.running = false;
    st.irqEnabled = false;

    if (st.alarmId > 0)
    {
        cancel_alarm(st.alarmId);
        st.alarmId = 0;
    }

    if (timerNum == TIMER16_1)
    {
        gpio_set_irq_enabled(PIN_EIB_RX, GPIO_IRQ_EDGE_FALL, false);
        drive_tx_idle_high();
    }
}

void Timer::prescaler(unsigned int factor)
{
    state_for(timerNum).prescaler = factor;
}

unsigned int Timer::prescaler() const
{
    return state_for(timerNum).prescaler;
}

void Timer::start()
{
    auto& st = state_for(timerNum);
    st.running = true;
    st.startUs = now_us();
    schedule_next_alarm(timerNum);
}

void Timer::stop()
{
    auto& st = state_for(timerNum);
    st.running = false;
    if (st.alarmId > 0)
    {
        cancel_alarm(st.alarmId);
        st.alarmId = 0;
    }
}

void Timer::restart()
{
    auto& st = state_for(timerNum);
    st.running = true;
    st.startUs = now_us();
    st.hasAcceptedCapture = false;
    st.lastAcceptedCapture = 0;
    schedule_next_alarm(timerNum);
}

void Timer::reset()
{
    auto& st = state_for(timerNum);
    st.startUs = now_us();
    st.hasAcceptedCapture = false;
    st.lastAcceptedCapture = 0;
    schedule_next_alarm(timerNum);
}

unsigned int Timer::value() const
{
    return current_value(timerNum);
}

void Timer::value(unsigned int val)
{
    auto& st = state_for(timerNum);
    st.startUs = now_us() - val;
    schedule_next_alarm(timerNum);
}

void Timer::interrupts()
{
    auto& st = state_for(timerNum);
    st.irqEnabled = true;

    if (timerNum == TIMER16_1)
    {
        bool enableCap0 = (st.captureModeValue[CAP0] & INTERRUPT) &&
                          (st.captureModeValue[CAP0] & FALLING_EDGE);

        gpio_set_irq_enabled_with_callback(PIN_EIB_RX, GPIO_IRQ_EDGE_FALL, enableCap0, &gpio_capture_callback);
    }

    schedule_next_alarm(timerNum);
}

void Timer::noInterrupts()
{
    auto& st = state_for(timerNum);
    st.irqEnabled = false;

    if (timerNum == TIMER16_1)
    {
        gpio_set_irq_enabled(PIN_EIB_RX, GPIO_IRQ_EDGE_FALL, false);
    }
}

int Timer::flags() const
{
    return (int)state_for(timerNum).flags;
}

void Timer::resetFlags()
{
    state_for(timerNum).flags = 0;
}

void Timer::resetFlag(TimerMatch match)
{
    state_for(timerNum).flags &= ~(1u << match);
}

void Timer::resetFlag(TimerCapture capture)
{
    state_for(timerNum).flags &= ~(16u << capture);
}

bool Timer::flag(TimerMatch match) const
{
    return (state_for(timerNum).flags & (1u << match)) != 0;
}

bool Timer::flag(TimerCapture capture) const
{
    return (state_for(timerNum).flags & (16u << capture)) != 0;
}

void Timer::matchMode(int channel, int mode)
{
    state_for(timerNum).matchModeValue[channel] = mode;
    schedule_next_alarm(timerNum);
}

int Timer::matchMode(int channel) const
{
    return state_for(timerNum).matchModeValue[channel];
}

void Timer::match(int channel, unsigned int val)
{
    state_for(timerNum).matchValue[channel] = val;
    schedule_next_alarm(timerNum);
}

unsigned int Timer::match(int channel) const
{
    return state_for(timerNum).matchValue[channel];
}

void Timer::captureMode(int channel, int mode)
{
    auto& st = state_for(timerNum);
    st.captureModeValue[channel] = mode;

    if (timerNum == TIMER16_1 && channel == CAP0)
    {
        bool enable = st.irqEnabled && (mode & INTERRUPT) && (mode & FALLING_EDGE);
        gpio_set_irq_enabled_with_callback(PIN_EIB_RX, GPIO_IRQ_EDGE_FALL, enable, &gpio_capture_callback);
    }
}

int Timer::captureMode(int channel) const
{
    return state_for(timerNum).captureModeValue[channel];
}

unsigned int Timer::capture(int channel) const
{
    return state_for(timerNum).captureValueArr[channel];
}

void Timer::pwmEnable(int channel)
{
    state_for(timerNum).pwmEnabled[channel] = true;
    schedule_next_alarm(timerNum);
}

void Timer::pwmDisable(int channel)
{
    state_for(timerNum).pwmEnabled[channel] = false;
    schedule_next_alarm(timerNum);
}

void Timer::counterMode(int mode, int clearMode)
{
    (void)mode;
    (void)clearMode;
}

void Timer::matchModePinConfig(int channel, int mode)
{
    (void)channel;
    (void)mode;
}

bool Timer::getMatchChannelLevel(int channel)
{
    return state_for(timerNum).matchLevel[channel];
}

void Timer::setIRQPriority(uint32_t newPriority)
{
    (void)newPriority;
}

#else

#define SYSTICK_ENABLED             ((SysTick->CTRL &  SysTick_CTRL_ENABLE_Msk) == SysTick_CTRL_ENABLE_Msk)
#define SYSTICK_INTERRUPT_ENABLED   ((SysTick->CTRL &  SysTick_CTRL_TICKINT_Msk) == SysTick_CTRL_TICKINT_Msk)

static volatile unsigned int systemTime = 0;
static LPC_TMR_TypeDef* const timers[4] = { LPC_TMR16B0, LPC_TMR16B1, LPC_TMR32B0, LPC_TMR32B1 };

void delay(unsigned int msec)
{
#ifndef IAP_EMULATION
    if ((!SYSTICK_ENABLED) && (!SYSTICK_INTERRUPT_ENABLED))
    {
        fatalError();
    }

    if (isInsideInterrupt())
    {
        unsigned int maxDelayMs = MAX_DELAY_MILLISECONDS;
        while (msec > maxDelayMs)
        {
            delayMicroseconds(maxDelayMs * 1000);
            msec -= (maxDelayMs);
        }

        delayMicroseconds(msec * 1000);
        return;
    }
#endif

    unsigned int lastSystemTime = systemTime;
    while (msec)
    {
        if (lastSystemTime == systemTime)
        {
            __WFI();
        }
        else
        {
            lastSystemTime = systemTime;
            --msec;
        }
    }
}

#ifndef IAP_EMULATION
void delayMicroseconds(unsigned int usec)
{
    uint16_t lastSystemTickValue = SysTick->VAL;
    uint16_t sysTickValue;
    int elapsedTicks;
    int ticksToWait = 1;

    if (usec > MIN_DELAY_MICROSECONDS)
    {
        if (usec > MAX_DELAY_MICROSECONDS)
        {
            fatalError();
        }
        ticksToWait = (usec - MIN_DELAY_MICROSECONDS) * (SystemCoreClock / 1000000);
    }

    while (ticksToWait > 0)
    {
        sysTickValue = SysTick->VAL;
        elapsedTicks = lastSystemTickValue - sysTickValue;
        if (elapsedTicks < 0)
        {
            elapsedTicks += SysTick->LOAD;
        }
        ticksToWait -= elapsedTicks;
        lastSystemTickValue = sysTickValue;
    }
}
#endif

#ifdef IAP_EMULATION
void setMillis(unsigned int newSystemTime)
{
    systemTime = newSystemTime;
}
#endif

unsigned int millis()
{
    return systemTime;
}

unsigned int elapsed(unsigned int ref)
{
    return millis() - ref;
}

Timer timer16_0(TIMER16_0);
Timer timer16_1(TIMER16_1);
Timer timer32_0(TIMER32_0);
Timer timer32_1(TIMER32_1);

Timer::Timer(byte aTimerNum)
{
    timerNum = aTimerNum;
    timer = timers[aTimerNum];
}

void Timer::begin()
{
    LPC_SYSCON->SYSAHBCLKCTRL |= 1 << (7 + timerNum);
    timer->EMR = 0;
    timer->MCR = 0;
    timer->CCR = 0;
}

void Timer::matchMode(int channel, int mode)
{
    int offset = channel * 3;
    timer->MCR = (timer->MCR & ~(7 << offset)) | ((mode & 7) << offset);
    matchModePinConfig(channel, mode);
}

int Timer::matchMode(int channel) const
{
    int offset = channel * 3;
    int mode = (timer->MCR >> offset) & 7;
    offset = channel << 1;
    mode |= (timer->EMR >> offset) & 0x30;
    return mode;
}

void Timer::captureMode(int channel, int mode)
{
    short offset = channel * 3;
    short val = (mode >> 6) & 3;
    if (mode & INTERRUPT)
        val |= 4;

    timer->CCR = (timer->CCR & ~(7 << offset)) | (val << offset);
}

int Timer::captureMode(int channel) const
{
    int mode = ((timer->CCR >> (channel * 3)) & 7) << 6;

    if (mode & 0x100)
    {
        mode &= 0xc0;
        mode |= INTERRUPT;
    }

    return mode;
}

void Timer::counterMode(int mode, int clearMode)
{
    int config = 0;

    if (mode & RISING_EDGE)  config |= 0x1;
    if (mode & FALLING_EDGE) config |= 0x2;
    if (mode & CAP1)         config |= 0x4;

    if (clearMode)
    {
        config |= 0x10;

        if (clearMode & (CAP1 | FALLING_EDGE))
            config |= 0x60;
        else if (clearMode & (CAP1 | RISING_EDGE))
            config |= 0x40;
        else if (clearMode & (CAP0 | FALLING_EDGE))
            config |= 0x20;
    }

    timer->CTCR = config;
}

void Timer::setIRQPriority(uint32_t newPriority)
{
    if (this == &timer16_0)
        NVIC_SetPriority(TIMER_16_0_IRQn, newPriority);
    else if (this == &timer16_1)
        NVIC_SetPriority(TIMER_16_1_IRQn, newPriority);
    else if (this == &timer32_0)
        NVIC_SetPriority(TIMER_32_0_IRQn, newPriority);
    else if (this == &timer32_1)
        NVIC_SetPriority(TIMER_32_1_IRQn, newPriority);
}

extern "C" void SysTick_Handler()
{
    ++systemTime;
}

#endif