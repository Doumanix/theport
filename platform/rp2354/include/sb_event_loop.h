#pragma once

#include <cstddef>
#include <cstdint>

using SbPollHandler = void(*)(void* user);

bool sb_event_loop_add_poll_handler(SbPollHandler handler, void* user = nullptr);
void sb_event_loop_poll_once();
size_t sb_event_loop_handler_count();

