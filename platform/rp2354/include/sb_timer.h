#pragma once

#include <stdint.h>

typedef void (*SbTimerCallback)(void* user);

struct SbTimer {
    uint32_t deadline;
    uint32_t period;
    bool periodic;
    bool active;
    SbTimerCallback cb;
    void* user;
};

void sb_timer_init(SbTimer* t,
                   uint32_t delay_ms,
                   bool periodic,
                   SbTimerCallback cb,
                   void* user);

void sb_timer_cancel(SbTimer* t);

void sb_timer_poll();
