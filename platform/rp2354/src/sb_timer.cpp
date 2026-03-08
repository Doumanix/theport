#include "sb_timer.h"
#include "sb_time.h"

namespace {

constexpr int MAX_TIMERS = 16;

SbTimer* g_timers[MAX_TIMERS];

}

void sb_timer_init(SbTimer* t,
                   uint32_t delay_ms,
                   bool periodic,
                   SbTimerCallback cb,
                   void* user) {

    t->deadline = sb_millis() + delay_ms;
    t->period = delay_ms;
    t->periodic = periodic;
    t->active = true;
    t->cb = cb;
    t->user = user;

    for (int i = 0; i < MAX_TIMERS; ++i) {
        if (!g_timers[i]) {
            g_timers[i] = t;
            return;
        }
    }
}

void sb_timer_cancel(SbTimer* t) {
    t->active = false;
}

void sb_timer_poll() {

    const uint32_t now = sb_millis();

    for (int i = 0; i < MAX_TIMERS; ++i) {

        SbTimer* t = g_timers[i];

        if (!t || !t->active)
            continue;

        if ((int32_t)(now - t->deadline) >= 0) {

            if (t->cb)
                t->cb(t->user);

            if (t->periodic) {
                t->deadline = now + t->period;
            } else {
                t->active = false;
            }
        }
    }
}
