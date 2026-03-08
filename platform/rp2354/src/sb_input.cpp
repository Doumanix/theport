#include "sb_input.h"

#include "sb_gpio.h"
#include "sb_irq.h"
#include "sb_time.h"

#include "hardware/gpio.h"

namespace {

struct InputSlot {
    bool used = false;
    bool state = false;
    uint32_t last_change = 0;
    uint32_t debounce_ms = 0;

    SbInputCallback cb = nullptr;
    void* user = nullptr;
};

InputSlot g_inputs[NUM_BANK0_GPIOS];

void irq_handler(uint8_t pin, uint32_t events, void* user) {
    (void)events;
    (void)user;

    if (pin >= NUM_BANK0_GPIOS) {
        return;
    }

    InputSlot& s = g_inputs[pin];
    if (!s.used) {
        return;
    }

    const uint32_t now = sb_millis();

    if ((now - s.last_change) < s.debounce_ms) {
        return;
    }

    bool level = gpio_get(pin);

    if (level != s.state) {
        s.state = level;
        s.last_change = now;

        if (s.cb) {
            s.cb(pin, level, s.user);
        }
    }
}

}

bool sb_input_init(const SbInputConfig& cfg,
                   SbInputCallback cb,
                   void* user) {

    if (cfg.pin >= NUM_BANK0_GPIOS) {
        return false;
    }

    InputSlot& s = g_inputs[cfg.pin];

    s.used = true;
    s.debounce_ms = cfg.debounce_ms;
    s.cb = cb;
    s.user = user;

    sb_gpio_init(cfg.pin, SbGpioDir::In);

    if (cfg.pullup) {
        gpio_pull_up(cfg.pin);
    }

    s.state = gpio_get(cfg.pin);
    s.last_change = sb_millis();

    return sb_gpio_irq_set_handler(
        cfg.pin,
        SbIrqTrigger::Both,
        irq_handler,
        nullptr
    );
}

bool sb_input_get(uint8_t pin) {
    if (pin >= NUM_BANK0_GPIOS) {
        return false;
    }

    return gpio_get(pin);
}
