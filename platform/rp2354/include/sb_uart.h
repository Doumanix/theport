//
// Created by Christian on 08.03.2026.
//

#ifndef SELFBUS_RP2350_BRINGUP_SB_UART_H
#define SELFBUS_RP2350_BRINGUP_SB_UART_H

#endif //SELFBUS_RP2350_BRINGUP_SB_UART_H

#pragma once
#include <cstddef>
#include <cstdint>

struct SbUartConfig {
    uint32_t baud = 115200;
    uint8_t tx_pin = 0;   // Pico 2 default UART0 TX: GP0
    uint8_t rx_pin = 1;   // Pico 2 default UART0 RX: GP1
};

void sb_uart0_init(const SbUartConfig& cfg);
bool sb_uart0_available();
int  sb_uart0_read_byte();                 // returns -1 if none
void sb_uart0_write(const void* data, size_t len);
void sb_uart0_write_byte(uint8_t b);