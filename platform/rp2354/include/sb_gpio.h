//
// Created by Christian on 08.03.2026.
//

#ifndef SELFBUS_RP2350_BRINGUP_SB_GPIO_H
#define SELFBUS_RP2350_BRINGUP_SB_GPIO_H

#endif //SELFBUS_RP2350_BRINGUP_SB_GPIO_H

#pragma once
#include <cstdint>

enum class SbGpioDir : uint8_t { In, Out };
enum class SbGpioPull : uint8_t { None, Up, Down };

void sb_gpio_init(uint8_t pin, SbGpioDir dir, SbGpioPull pull = SbGpioPull::None);
bool sb_gpio_read(uint8_t pin);
void sb_gpio_write(uint8_t pin, bool level);