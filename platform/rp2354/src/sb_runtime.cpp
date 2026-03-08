#include "sb_runtime.h"

#include "sb_event_loop.h"
#include "sb_timer.h"

static void poll_timers(void*) {
    sb_timer_poll();
}

void sb_runtime_init() {
    sb_event_loop_add_poll_handler(poll_timers, nullptr);
}

void sb_runtime_poll() {
    sb_event_loop_poll_once();
}
