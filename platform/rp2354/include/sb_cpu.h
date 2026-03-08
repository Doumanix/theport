#pragma once

#include <cstdint>

// Disable interrupts and return previous interrupt state token.
uint32_t sb_irq_disable();

// Restore interrupt state returned by sb_irq_disable().
void sb_irq_restore(uint32_t state);

void sb_wait_for_interrupt();
void sb_system_reset();
