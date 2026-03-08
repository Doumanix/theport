#include "sb_button.h"

#include "sb_input.h"
#include "sb_time.h"

#include "hardware/gpio.h"

namespace {

enum class ButtonState : uint8_t {
    Idle,
    Down,
    WaitSecondPress,
    DownSecond
};

struct ButtonSlot {
    bool used = false;
    bool active_low = true;
    bool pressed = false;
    bool long_sent = false;

    uint32_t press_time = 0;
    uint32_t release_time = 0;

    uint32_t long_press_ms = 500;
    uint32_t double_click_ms = 250;

    ButtonState state = ButtonState::Idle;

    SbButtonCallback cb = nullptr;
    void* user = nullptr;
};

ButtonSlot g_buttons[NUM_BANK0_GPIOS];

bool raw_to_pressed(uint8_t pin, bool active_low) {
    const bool raw = gpio_get(pin);
    return active_low ? !raw : raw;
}

void emit_event(uint8_t pin, SbButtonEvent event) {
    ButtonSlot& s = g_buttons[pin];
    if (s.cb) {
        s.cb(pin, event, s.user);
    }
}

void input_changed(uint8_t pin, bool state, void* user) {
    (void)state;
    (void)user;

    if (pin >= NUM_BANK0_GPIOS) {
        return;
    }

    ButtonSlot& s = g_buttons[pin];
    if (!s.used) {
        return;
    }

    const uint32_t now = sb_millis();
    const bool pressed = raw_to_pressed(pin, s.active_low);
    s.pressed = pressed;

    switch (s.state) {
        case ButtonState::Idle:
            if (pressed) {
                s.state = ButtonState::Down;
                s.press_time = now;
                s.long_sent = false;
                emit_event(pin, SbButtonEvent::Press);
            }
            break;

        case ButtonState::Down:
            if (!pressed) {
                s.release_time = now;
                s.state = s.long_sent ? ButtonState::Idle : ButtonState::WaitSecondPress;
                emit_event(pin, SbButtonEvent::Release);
            }
            break;

        case ButtonState::WaitSecondPress:
            if (pressed) {
                s.state = ButtonState::DownSecond;
                s.press_time = now;
                s.long_sent = false;
                emit_event(pin, SbButtonEvent::Press);
            }
            break;

        case ButtonState::DownSecond:
            if (!pressed) {
                emit_event(pin, SbButtonEvent::Release);
                if (!s.long_sent) {
                    emit_event(pin, SbButtonEvent::Double);
                }
                s.state = ButtonState::Idle;
            }
            break;
    }
}

}  // namespace

bool sb_button_init(const SbButtonConfig& cfg,
                    SbButtonCallback cb,
                    void* user) {
    if (cfg.pin >= NUM_BANK0_GPIOS || !cb) {
        return false;
    }

    ButtonSlot& s = g_buttons[cfg.pin];
    s.used = true;
    s.active_low = cfg.active_low;
    s.long_press_ms = cfg.long_press_ms;
    s.double_click_ms = cfg.double_click_ms;
    s.cb = cb;
    s.user = user;
    s.state = ButtonState::Idle;
    s.long_sent = false;
    s.pressed = false;
    s.press_time = 0;
    s.release_time = 0;

    SbInputConfig icfg{};
    icfg.pin = cfg.pin;
    icfg.pullup = cfg.pullup;
    icfg.debounce_ms = cfg.debounce_ms;

    return sb_input_init(icfg, input_changed, nullptr);
}

bool sb_button_is_pressed(uint8_t pin) {
    if (pin >= NUM_BANK0_GPIOS || !g_buttons[pin].used) {
        return false;
    }
    return g_buttons[pin].pressed;
}

void sb_button_poll() {
    const uint32_t now = sb_millis();

    for (uint32_t pin = 0; pin < NUM_BANK0_GPIOS; ++pin) {
        ButtonSlot& s = g_buttons[pin];
        if (!s.used) {
            continue;
        }

        switch (s.state) {
            case ButtonState::Idle:
                break;

            case ButtonState::Down:
                if (s.pressed && !s.long_sent &&
                    (uint32_t)(now - s.press_time) >= s.long_press_ms) {
                    s.long_sent = true;
                    emit_event(static_cast<uint8_t>(pin), SbButtonEvent::Long);
                }
                break;

            case ButtonState::WaitSecondPress:
                if ((uint32_t)(now - s.release_time) >= s.double_click_ms) {
                    emit_event(static_cast<uint8_t>(pin), SbButtonEvent::Short);
                    s.state = ButtonState::Idle;
                }
                break;

            case ButtonState::DownSecond:
                if (s.pressed && !s.long_sent &&
                    (uint32_t)(now - s.press_time) >= s.long_press_ms) {
                    s.long_sent = true;
                    emit_event(static_cast<uint8_t>(pin), SbButtonEvent::Long);
                }
                break;
        }
    }
}