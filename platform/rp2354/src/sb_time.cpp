//
// Created by Christian on 08.03.2026.
//
#include "sb_time.h"
#include "pico/time.h"
#include "pico/stdlib.h"

uint32_t sb_millis() {
    return to_ms_since_boot(get_absolute_time());
}

uint32_t sb_micros() {
    return (uint32_t)to_us_since_boot(get_absolute_time());
}

void sb_delay_ms(uint32_t ms) {
    sleep_ms(ms);
}