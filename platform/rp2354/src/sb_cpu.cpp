#include "sb_cpu.h"

#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/platform.h"

uint32_t sb_irq_disable() {
    return save_and_disable_interrupts();
}

void sb_irq_restore(uint32_t state) {
    restore_interrupts(state);
}

void sb_wait_for_interrupt() {
    __wfi();
}

void sb_system_reset() {
    watchdog_reboot(0, 0, 0);
    while (true) {
    }
}