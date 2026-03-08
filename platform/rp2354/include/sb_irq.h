#pragma once

#include <cstdint>

enum class SbIrqTrigger : uint8_t {
    Rising,
    Falling,
    Both
};

using SbGpioIrqHandler = void(*)(uint8_t pin, uint32_t events, void* user_data);

void sb_gpio_irq_init();

bool sb_gpio_irq_set_handler(uint8_t pin,
                             SbIrqTrigger trigger,
                             SbGpioIrqHandler handler,
                             void* user_data = nullptr);

void sb_gpio_irq_enable(uint8_t pin);
void sb_gpio_irq_disable(uint8_t pin);
void sb_gpio_irq_remove_handler(uint8_t pin);

constexpr uint32_t SB_GPIO_IRQ_EVENT_RISE = 0x01;
constexpr uint32_t SB_GPIO_IRQ_EVENT_FALL = 0x02;

