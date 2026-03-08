#include "sb_irq.h"

#include "hardware/gpio.h"

namespace {

struct GpioIrqSlot {
    SbGpioIrqHandler handler = nullptr;
    void* user_data = nullptr;
    uint32_t event_mask = 0;
};

GpioIrqSlot g_slots[NUM_BANK0_GPIOS];
bool g_irq_callback_installed = false;

uint32_t to_pico_event_mask(SbIrqTrigger trigger) {
    switch (trigger) {
        case SbIrqTrigger::Rising:
            return GPIO_IRQ_EDGE_RISE;
        case SbIrqTrigger::Falling:
            return GPIO_IRQ_EDGE_FALL;
        case SbIrqTrigger::Both:
            return GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
        default:
            return 0;
    }
}

uint32_t to_sb_event_mask(uint32_t events) {
    uint32_t out = 0;
    if (events & GPIO_IRQ_EDGE_RISE) {
        out |= SB_GPIO_IRQ_EVENT_RISE;
    }
    if (events & GPIO_IRQ_EDGE_FALL) {
        out |= SB_GPIO_IRQ_EVENT_FALL;
    }
    return out;
}

void gpio_irq_callback(uint gpio, uint32_t events) {
    if (gpio >= NUM_BANK0_GPIOS) {
        return;
    }

    const GpioIrqSlot& slot = g_slots[gpio];
    if (!slot.handler) {
        return;
    }

    slot.handler(static_cast<uint8_t>(gpio), to_sb_event_mask(events), slot.user_data);
}

}  // namespace

void sb_gpio_irq_init() {
    if (!g_irq_callback_installed) {
        gpio_set_irq_callback(gpio_irq_callback);
        irq_set_enabled(IO_IRQ_BANK0, true);
        g_irq_callback_installed = true;
    }
}

bool sb_gpio_irq_set_handler(uint8_t pin,
                             SbIrqTrigger trigger,
                             SbGpioIrqHandler handler,
                             void* user_data) {
    if (pin >= NUM_BANK0_GPIOS || !handler) {
        return false;
    }

    sb_gpio_irq_init();

    g_slots[pin].handler = handler;
    g_slots[pin].user_data = user_data;
    g_slots[pin].event_mask = to_pico_event_mask(trigger);

    gpio_set_irq_enabled(pin, g_slots[pin].event_mask, true);
    return true;
}

void sb_gpio_irq_enable(uint8_t pin) {
    if (pin >= NUM_BANK0_GPIOS || !g_slots[pin].handler) {
        return;
    }
    gpio_set_irq_enabled(pin, g_slots[pin].event_mask, true);
}

void sb_gpio_irq_disable(uint8_t pin) {
    if (pin >= NUM_BANK0_GPIOS) {
        return;
    }
    gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
}

void sb_gpio_irq_remove_handler(uint8_t pin) {
    if (pin >= NUM_BANK0_GPIOS) {
        return;
    }
    sb_gpio_irq_disable(pin);
    g_slots[pin] = {};
}