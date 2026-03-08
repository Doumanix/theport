#pragma once

#include <cstdint>

enum class SbButtonEvent : uint8_t {
    Press,
    Release,
    Short,
    Long,
    Double
};

using SbButtonCallback = void(*)(uint8_t pin, SbButtonEvent event, void* user);

struct SbButtonConfig {
    uint8_t pin;
    bool pullup = true;
    uint32_t debounce_ms = 20;
    uint32_t long_press_ms = 500;
    uint32_t double_click_ms = 250;
    bool active_low = true;
};

bool sb_button_init(const SbButtonConfig& cfg,
                    SbButtonCallback cb,
                    void* user = nullptr);

bool sb_button_is_pressed(uint8_t pin);

// Must be called periodically from the main loop.
void sb_button_poll();
