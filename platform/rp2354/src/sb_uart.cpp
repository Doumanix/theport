//
// Created by Christian on 08.03.2026.
//
#include "sb_uart.h"

#include "hardware/uart.h"
#include "hardware/gpio.h"

void sb_uart0_init(const SbUartConfig& cfg) {
    uart_init(uart0, cfg.baud);

    gpio_set_function(cfg.tx_pin, GPIO_FUNC_UART);
    gpio_set_function(cfg.rx_pin, GPIO_FUNC_UART);

    // 8N1 ist Default; explizit schadet nicht:
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, true);
}

bool sb_uart0_available() {
    return uart_is_readable(uart0);
}

int sb_uart0_read_byte() {
    if (!uart_is_readable(uart0)) return -1;
    return uart_getc(uart0);
}

void sb_uart0_write(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; i++) {
        uart_putc_raw(uart0, p[i]);
    }
}

void sb_uart0_write_byte(uint8_t b) {
    uart_putc_raw(uart0, b);
}