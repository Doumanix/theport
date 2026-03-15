// Created by Christian on 08.03.2026.

#ifndef SELFBUS_RP2350_BRINGUP_SB_GPIO_H
#define SELFBUS_RP2350_BRINGUP_SB_GPIO_H

#pragma once

#include <stdint.h>

#ifndef PIO0_0
#define PIO0_0 0
#define PIO0_1 1
#define PIO0_2 2
#define PIO0_3 3
#define PIO0_4 4
#define PIO0_5 5
#define PIO0_6 6
#define PIO0_7 7
#define PIO0_8 8
#define PIO0_9 9
#define PIO0_10 10
#define PIO0_11 11
#define PIO0_12 12
#define PIO0_13 13
#define PIO0_14 14
#define PIO0_15 15
#define PIO0_16 16
#define PIO0_17 17
#define PIO0_18 18
#define PIO0_19 19
#define PIO0_20 20
#define PIO0_21 21
#define PIO0_22 22
#define PIO0_23 23
#define PIO0_24 24
#define PIO0_25 25
#define PIO0_26 26
#define PIO0_27 27
#define PIO0_28 28
#define PIO0_29 29
#endif

/*
 * First concrete RP2354 hardware mapping
 */
#ifndef PIN_EIB_TX
#define PIN_EIB_TX  PIO0_15
#endif

#ifndef PIN_EIB_RX
#define PIN_EIB_RX  PIO0_16
#endif

#ifndef PIN_PROG
#define PIN_PROG    PIO0_22
#endif

#ifndef PIO_SCL
#define PIO_SCL     PIO0_5
#endif

#ifndef PIO_SDA
#define PIO_SDA     PIO0_4
#endif

/*
 * Debug UART mapping
 */
#ifndef PIN_TX
#define PIN_TX      PIO0_0
#endif

#ifndef PIN_RX
#define PIN_RX      PIO0_1
#endif

/*
 * Optional / currently unused on first RP2354 hardware
 */
#ifndef PIN_INFO
#define PIN_INFO    (-1)
#endif

#ifndef PIN_RUN
#define PIN_RUN     (-1)
#endif

#ifndef PIN_VBUS
#define PIN_VBUS    (-1)
#endif

#ifndef PIN_APRG
#define PIN_APRG    PIN_PROG
#endif

enum class SbGpioDir : uint8_t
{
    In,
    Out
};

enum class SbGpioPull : uint8_t
{
    None,
    Up,
    Down
};

void sb_gpio_init(uint8_t pin, SbGpioDir dir, SbGpioPull pull = SbGpioPull::None);
bool sb_gpio_read(uint8_t pin);
void sb_gpio_write(uint8_t pin, bool level);

#endif // SELFBUS_RP2350_BRINGUP_SB_GPIO_H