//
// Created by Christian on 08.03.2026.
//
#include "sb_gpio.h"
#include "hardware/gpio.h"

void sb_gpio_init(uint8_t pin, SbGpioDir dir, SbGpioPull pull) {
    gpio_init(pin);
    gpio_set_dir(pin, dir == SbGpioDir::Out);

    // Pulls
    switch (pull) {
    case SbGpioPull::Up:   gpio_pull_up(pin); break;
    case SbGpioPull::Down: gpio_pull_down(pin); break;
    case SbGpioPull::None: gpio_disable_pulls(pin); break;
    }
}

bool sb_gpio_read(uint8_t pin) {
    return gpio_get(pin);
}

void sb_gpio_write(uint8_t pin, bool level) {
    gpio_put(pin, level);
}