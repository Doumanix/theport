#pragma once

#include <cstdint>

using SbInputCallback = void(*)(uint8_t pin, bool state, void* user);

struct SbInputConfig {
    uint8_t pin;
    bool pullup;
    uint32_t debounce_ms;
};

bool sb_input_init(const SbInputConfig& cfg,
                   SbInputCallback cb,
                   void* user = nullptr);

bool sb_input_get(uint8_t pin);

