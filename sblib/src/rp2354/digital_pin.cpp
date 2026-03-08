/*
 * digital_pin.cpp - Digital input/output pin functions for RP2354.
 *
 * RP2354 backend for the existing sblib digital pin API.
 */

#include "sblib/digital_pin.h"
#include "sblib/platform.h"
#include "sblib/ioports.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"

namespace
{
    static constexpr unsigned int kMaxGpio = 48;

    static uint32_t s_irq_event_mask[kMaxGpio];
    static bool s_irq_enabled[kMaxGpio];
    static bool s_irq_dispatcher_installed = false;

    static inline unsigned int gpio_num_from_pin(int pin)
    {
        return sblibGpioNumber(pin);
    }

    static inline bool valid_pin(int pin)
    {
        return sblibValidGpio(pin);
    }

    static inline uint32_t irq_events_from_mode(int mode)
    {
        switch (mode & 0x300)
        {
            case INTERRUPT_ON_FALLING:
                return GPIO_IRQ_EDGE_FALL;
            case INTERRUPT_ON_RISING:
                return GPIO_IRQ_EDGE_RISE;
            case INTERRUPT_ON_CHANGE:
                return GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
            default:
                return 0;
        }
    }

    static inline void apply_pull_mode(unsigned int gpio, int mode)
    {
        gpio_disable_pulls(gpio);

        if (mode & PULL_UP)
            gpio_pull_up(gpio);
        else if (mode & PULL_DOWN)
            gpio_pull_down(gpio);
    }

    static inline bool is_output_mode(int mode)
    {
        return (mode & OUTPUT) == OUTPUT;
    }

    static inline gpio_function_t function_from_pinfunc(short func)
    {
        switch (func)
        {
            case PF_PIO:
            case PF_NONE:
                return GPIO_FUNC_SIO;

            case PF_RXD:
            case PF_TXD:
            case PF_RTS:
            case PF_CTS:
            case PF_DTR:
            case PF_DSR:
            case PF_DCD:
            case PF_RI:
                return GPIO_FUNC_UART;

            case PF_SCK:
            case PF_MISO:
            case PF_MOSI:
            case PF_SSEL:
                return GPIO_FUNC_SPI;

            case PF_SDA:
            case PF_SCL:
                return GPIO_FUNC_I2C;

            default:
                return GPIO_FUNC_NULL;
        }
    }

    static void gpio_irq_dispatcher(uint gpio, uint32_t events)
    {
        (void) events;

        if (gpio >= kMaxGpio)
            return;

        /*
         * sblib expects the actual interrupt masking/unmasking to happen via
         * pinEnableInterrupt()/pinDisableInterrupt(). The user ISR hookup is
         * handled elsewhere in the library, just like on LPC.
         *
         * We therefore only keep the GPIO IRQ armed here.
         */
    }

    static void ensure_irq_dispatcher_installed()
    {
        if (s_irq_dispatcher_installed)
            return;

        gpio_set_irq_callback(gpio_irq_dispatcher);
        irq_set_enabled(IO_IRQ_BANK0, true);
        s_irq_dispatcher_installed = true;
    }
}

const int portMask[12] =
{
    1 << 0,  1 << 1,  1 << 2,  1 << 3,
    1 << 4,  1 << 5,  1 << 6,  1 << 7,
    1 << 8,  1 << 9,  1 << 10, 1 << 11
};

short getPinFunctionNumber(int pin, short func)
{
    (void) pin;

    if (func == PF_PIO || func == PF_NONE)
        return 0;

    switch (func)
    {
        case PF_RXD:
        case PF_TXD:
        case PF_RTS:
        case PF_CTS:
        case PF_DTR:
        case PF_DSR:
        case PF_DCD:
        case PF_RI:
        case PF_SCK:
        case PF_MISO:
        case PF_MOSI:
        case PF_SSEL:
        case PF_SDA:
        case PF_SCL:
            return 1;

        default:
            return -1;
    }
}

void pinDirection(int pin, int output)
{
    if (!valid_pin(pin))
        return;

    const unsigned int gpio = gpio_num_from_pin(pin);

    gpio_init(gpio);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
    gpio_set_dir(gpio, output ? GPIO_OUT : GPIO_IN);
}

void pinMode(int pin, int mode)
{
    if (!valid_pin(pin))
        return;

    const unsigned int gpio = gpio_num_from_pin(pin);
    const short requested_func = (short) ((mode >> 18) & 0x1f);

    gpio_init(gpio);

    if (requested_func == 0 || requested_func == PF_PIO)
    {
        gpio_set_function(gpio, GPIO_FUNC_SIO);
    }
    else
    {
        const gpio_function_t fn = function_from_pinfunc(requested_func);

        if (fn == GPIO_FUNC_NULL)
        {
            /*
             * Unsupported alternate function on RP2354 backend.
             * Fall back to plain GPIO to keep behavior deterministic.
             */
            gpio_set_function(gpio, GPIO_FUNC_SIO);
        }
        else
        {
            gpio_set_function(gpio, fn);
        }
    }

    if (is_output_mode(mode))
        gpio_set_dir(gpio, GPIO_OUT);
    else
        gpio_set_dir(gpio, GPIO_IN);

    apply_pull_mode(gpio, mode);

    /*
     * Open-drain is not natively modeled in the original LPC path either via a
     * separate runtime abstraction. For now keep the pin as normal GPIO.
     * If needed later, this can be emulated by switching direction dynamically.
     */
}

void pinInterruptMode(int pin, int mode)
{
    if (!valid_pin(pin))
        return;

    const unsigned int gpio = gpio_num_from_pin(pin);
    const uint32_t events = irq_events_from_mode(mode);

    gpio_init(gpio);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
    gpio_set_dir(gpio, GPIO_IN);
    apply_pull_mode(gpio, mode);

    s_irq_event_mask[gpio] = events;

    if (events == 0)
    {
        gpio_set_irq_enabled(gpio,
                             GPIO_IRQ_EDGE_RISE |
                             GPIO_IRQ_EDGE_FALL |
                             GPIO_IRQ_LEVEL_LOW |
                             GPIO_IRQ_LEVEL_HIGH,
                             false);
        s_irq_enabled[gpio] = false;
        return;
    }

    ensure_irq_dispatcher_installed();

    /*
     * Match LPC behavior: pinInterruptMode() configures the trigger, while the
     * actual interrupt enable is controlled separately by pinEnableInterrupt().
     */
    gpio_set_irq_enabled(gpio,
                         GPIO_IRQ_EDGE_RISE |
                         GPIO_IRQ_EDGE_FALL |
                         GPIO_IRQ_LEVEL_LOW |
                         GPIO_IRQ_LEVEL_HIGH,
                         false);

    s_irq_enabled[gpio] = false;

    if (mode & INTERRUPT_ENABLED)
        sb_pin_irq_enable(pin);
}

void sb_pin_irq_enable(int pin)
{
    if (!valid_pin(pin))
        return;

    const unsigned int gpio = gpio_num_from_pin(pin);
    const uint32_t events = s_irq_event_mask[gpio];

    if (!events)
        return;

    ensure_irq_dispatcher_installed();
    gpio_acknowledge_irq(gpio,
                         GPIO_IRQ_EDGE_RISE |
                         GPIO_IRQ_EDGE_FALL |
                         GPIO_IRQ_LEVEL_LOW |
                         GPIO_IRQ_LEVEL_HIGH);
    gpio_set_irq_enabled(gpio, events, true);
    s_irq_enabled[gpio] = true;
}

void sb_pin_irq_disable(int pin)
{
    if (!valid_pin(pin))
        return;

    const unsigned int gpio = gpio_num_from_pin(pin);

    gpio_set_irq_enabled(gpio,
                         GPIO_IRQ_EDGE_RISE |
                         GPIO_IRQ_EDGE_FALL |
                         GPIO_IRQ_LEVEL_LOW |
                         GPIO_IRQ_LEVEL_HIGH,
                         false);
    gpio_acknowledge_irq(gpio,
                         GPIO_IRQ_EDGE_RISE |
                         GPIO_IRQ_EDGE_FALL |
                         GPIO_IRQ_LEVEL_LOW |
                         GPIO_IRQ_LEVEL_HIGH);
    s_irq_enabled[gpio] = false;
}