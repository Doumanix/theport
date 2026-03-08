#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/stdio_usb.h"

#include "sb_time.h"
#include "sb_gpio.h"
#include "sb_button.h"
#include "sb_runtime.h"
#include "sb_event_loop.h"
#include "sb_timer.h"
#include "sb_uart.h"

static bool usb_cdc_is_connected() {
    // True if a host has opened the USB CDC connection.
    return stdio_usb_connected();
}

static void poll_buttons(void*) {
    sb_button_poll();
}

static SbTimer g_test_timer;

static void timer_cb(void*) {
    if (usb_cdc_is_connected()) {
        std::printf("timer tick\n");
    }
}


static void button_cb(uint8_t pin, SbButtonEvent ev, void*) {
    const char* name = "?";
    switch (ev) {
        case SbButtonEvent::Press:   name = "Press"; break;
        case SbButtonEvent::Release: name = "Release"; break;
        case SbButtonEvent::Short:   name = "Short"; break;
        case SbButtonEvent::Long:    name = "Long"; break;
        case SbButtonEvent::Double:  name = "Double"; break;
    }

    std::printf("button pin=%u event=%s\n", (unsigned)pin, name);
}

int main() {
    stdio_init_all();

#ifdef PICO_DEFAULT_LED_PIN
    sb_gpio_init(PICO_DEFAULT_LED_PIN, SbGpioDir::Out);
#endif

    // UART0 on GP0/GP1 (Pico 2 defaults)
    SbUartConfig uc{};
    uc.baud = 115200;
    uc.tx_pin = 0; // GP0
    uc.rx_pin = 1; // GP1
    sb_uart0_init(uc);

    // Test button on GP10:
    // - internal pull-up enabled
    // - button/switch connects GP10 to GND when pressed
    SbButtonConfig bcfg{};
    bcfg.pin = 10;
    bcfg.pullup = true;
    bcfg.active_low = true;
    bcfg.debounce_ms = 20;
    sb_button_init(bcfg, button_cb, nullptr);

    sb_event_loop_add_poll_handler(poll_buttons, nullptr);

    sb_timer_init(
        &g_test_timer,
        1000,
        true,
        timer_cb,
        nullptr);

    // Don't block startup waiting for PuTTY; LED should blink regardless.
    // We'll print the banner once a CDC connection appears.
    bool banner_printed = false;
    uint32_t last_alive = sb_millis();
#ifdef PICO_DEFAULT_LED_PIN
    // Blink LED independently from logging: 1 Hz (toggle every 500 ms)
    uint32_t last_led = last_alive;
    bool led_on = false;
#endif

    while (true) {
        const uint32_t now = sb_millis();

#ifdef PICO_DEFAULT_LED_PIN
        if ((uint32_t)(now - last_led) >= 500u) {
            last_led = now;
            led_on = !led_on;
            sb_gpio_write(PICO_DEFAULT_LED_PIN, led_on);
        }
#endif

        // Print banner once when a terminal connects (so PuTTY doesn't miss it).
        if (!banner_printed && usb_cdc_is_connected()) {
            std::printf("\nbringup: USB-CDC OK, UART0 on GP0/GP1 @115200\n");
            const char* banner = "UART0 alive (send chars, I echo them)\r\n";
            sb_uart0_write(banner, std::strlen(banner));
            banner_printed = true;
            last_alive = now; // align alive cadence after connect
        }

        // Periodic alive on USB-CDC (reconnect-friendly)
        if (banner_printed && (uint32_t)(now - last_alive) >= 1000u) {
            last_alive = now;
            std::printf("alive uptime=%lu ms\n", (unsigned long)now);
        }

        // UART0 echo + mirror to USB-CDC
        int ch = sb_uart0_read_byte();
        if (ch >= 0) {
            if (banner_printed) {
                std::printf("uart0 rx: 0x%02X '%c'\n",
                            (unsigned)ch,
                            (ch >= 32 && ch < 127) ? ch : '.');
            }
            sb_uart0_write_byte((uint8_t)ch);
        }

        sb_runtime_poll();
        sb_delay_ms(10);
    }
}