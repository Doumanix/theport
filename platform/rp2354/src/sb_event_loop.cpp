#include "sb_event_loop.h"

namespace {

struct PollSlot {
    SbPollHandler handler = nullptr;
    void* user = nullptr;
};

constexpr size_t kMaxPollHandlers = 16;
PollSlot g_slots[kMaxPollHandlers];
size_t g_count = 0;

}  // namespace

bool sb_event_loop_add_poll_handler(SbPollHandler handler, void* user) {
    if (!handler || g_count >= kMaxPollHandlers) {
        return false;
    }

    g_slots[g_count].handler = handler;
    g_slots[g_count].user = user;
    ++g_count;
    return true;
}

void sb_event_loop_poll_once() {
    for (size_t i = 0; i < g_count; ++i) {
        if (g_slots[i].handler) {
            g_slots[i].handler(g_slots[i].user);
        }
    }
}

size_t sb_event_loop_handler_count() {
    return g_count;
}

