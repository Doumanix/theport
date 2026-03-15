#include <sblib/serial.h>
#include <sblib/eib/bcu1.h>
#include <sblib/eib/bcu_default.h>
#include <sblib/eib/bus.h>
#include "pico/stdlib.h"

extern volatile unsigned int sb_rx_irq_count;
extern volatile unsigned int sb_rx_capture_count;
extern volatile unsigned int sb_rx_reject_short_count;
extern volatile unsigned int sb_rx_last_capture;
extern volatile unsigned int sb_rx_last_delta;
extern volatile unsigned int sb_tx_pwm_fire_count;
extern volatile unsigned int sb_tx_time_fire_count;

extern volatile unsigned int sb_bus_handle_count;
extern volatile unsigned int sb_bus_valid_frame_count;
extern volatile unsigned int sb_bus_process_tel_count;
extern volatile unsigned int sb_bus_accept_count;
extern volatile unsigned int sb_bus_reject_not_for_us_count;
extern volatile unsigned int sb_bus_reject_invalid_count;
extern volatile unsigned int sb_bus_reject_busy_count;
extern volatile unsigned int sb_bus_ack_wait_count;

extern volatile unsigned int sb_bus_rxerr_checksum_count;
extern volatile unsigned int sb_bus_rxerr_parity_count;
extern volatile unsigned int sb_bus_rxerr_stopbit_count;
extern volatile unsigned int sb_bus_rxerr_timing_count;
extern volatile unsigned int sb_bus_rxerr_length_count;
extern volatile unsigned int sb_bus_rxerr_preamble_count;
extern volatile unsigned int sb_bus_rxerr_invalid_tel_count;
extern volatile unsigned int sb_bus_rxerr_buffer_busy_count;
extern volatile unsigned int sb_bus_rxerr_last_mask;

static Serial dbg(1, 0);

static void write_text(const char* s)
{
    while (*s)
        dbg.write((byte)*s++);
}

static void write_dec(unsigned int v)
{
    char buf[16];
    int pos = 0;

    if (v == 0)
    {
        dbg.write((byte)'0');
        return;
    }

    while (v > 0 && pos < (int)sizeof(buf))
    {
        buf[pos++] = (char)('0' + (v % 10));
        v /= 10;
    }

    while (pos > 0)
        dbg.write((byte)buf[--pos]);
}

static void write_hex_nibble(byte v)
{
    v &= 0x0F;
    dbg.write((byte)(v < 10 ? ('0' + v) : ('A' + (v - 10))));
}

static void write_hex32(unsigned int v)
{
    write_hex_nibble((byte)(v >> 28));
    write_hex_nibble((byte)(v >> 24));
    write_hex_nibble((byte)(v >> 20));
    write_hex_nibble((byte)(v >> 16));
    write_hex_nibble((byte)(v >> 12));
    write_hex_nibble((byte)(v >> 8));
    write_hex_nibble((byte)(v >> 4));
    write_hex_nibble((byte)(v));
}

class TestBCU1 : public BCU1
{
public:
    using BcuDefault::setOwnAddress;

    void sendGroupWrite1Bit(uint16_t groupAddress, bool value)
    {
        static byte telegram[8];

        telegram[0] = 0xBC;
        telegram[1] = 0x00;
        telegram[2] = 0x00;
        telegram[3] = (byte)(groupAddress >> 8);
        telegram[4] = (byte)(groupAddress & 0xFF);
        telegram[5] = 0xF1;
        telegram[6] = 0x00;
        telegram[7] = value ? 0x81 : 0x80;

        bus->sendTelegram(telegram, 8);
    }
};

int main()
{
    dbg.begin(115200);
    sleep_ms(200);

    write_text("\r\nRP2354 RX/TX + BUS diagnostics\r\n");

    TestBCU1 bcu;

    write_text("before bcu.begin\r\n");
    bcu.begin(0x1234, 0x0001, 0x01);
    write_text("after bcu.begin\r\n");

    bcu.setOwnAddress(0x1250); // 1.2.80

    unsigned int last_irq = 0;
    unsigned int last_cap = 0;
    unsigned int last_rej = 0;
    unsigned int last_pwm = 0;
    unsigned int last_time = 0;

    unsigned int last_bus_handle = 0;
    unsigned int last_bus_valid = 0;
    unsigned int last_bus_process = 0;
    unsigned int last_bus_accept = 0;
    unsigned int last_bus_not_for_us = 0;
    unsigned int last_bus_invalid = 0;
    unsigned int last_bus_busy = 0;
    unsigned int last_bus_ack_wait = 0;

    unsigned int last_err_checksum = 0;
    unsigned int last_err_parity = 0;
    unsigned int last_err_stop = 0;
    unsigned int last_err_timing = 0;
    unsigned int last_err_length = 0;
    unsigned int last_err_preamble = 0;
    unsigned int last_err_invalid = 0;
    unsigned int last_err_busy = 0;

    uint32_t last_report = 0;
    uint32_t last_send = 0;

    while (true)
    {
        bcu.loop();

        const uint32_t now = to_ms_since_boot(get_absolute_time());

        if (!bcu.bus->sendingFrame() && (now - last_send) >= 5000)
        {
            last_send = now;
            write_text("TX: group write 1/0/11 = 0\r\n");
            // bcu.sendGroupWrite1Bit(0x080B, false);
        }

        if ((now - last_report) >= 1000)
        {
            last_report = now;

            write_text("rx_irq=");
            write_dec(sb_rx_irq_count);
            write_text(" (+");
            write_dec(sb_rx_irq_count - last_irq);
            write_text(") ");

            write_text("cap=");
            write_dec(sb_rx_capture_count);
            write_text(" (+");
            write_dec(sb_rx_capture_count - last_cap);
            write_text(") ");

            write_text("rej=");
            write_dec(sb_rx_reject_short_count);
            write_text(" (+");
            write_dec(sb_rx_reject_short_count - last_rej);
            write_text(") ");

            write_text("last_cap=");
            write_dec(sb_rx_last_capture);
            write_text(" ");

            write_text("last_delta=");
            write_dec(sb_rx_last_delta);
            write_text(" ");

            write_text("tx_pwm=");
            write_dec(sb_tx_pwm_fire_count);
            write_text(" (+");
            write_dec(sb_tx_pwm_fire_count - last_pwm);
            write_text(") ");

            write_text("tx_time=");
            write_dec(sb_tx_time_fire_count);
            write_text(" (+");
            write_dec(sb_tx_time_fire_count - last_time);
            write_text(") ");

            write_text("bus_handle=");
            write_dec(sb_bus_handle_count);
            write_text(" (+");
            write_dec(sb_bus_handle_count - last_bus_handle);
            write_text(") ");

            write_text("bus_valid=");
            write_dec(sb_bus_valid_frame_count);
            write_text(" (+");
            write_dec(sb_bus_valid_frame_count - last_bus_valid);
            write_text(") ");

            write_text("bus_proc=");
            write_dec(sb_bus_process_tel_count);
            write_text(" (+");
            write_dec(sb_bus_process_tel_count - last_bus_process);
            write_text(") ");

            write_text("bus_acc=");
            write_dec(sb_bus_accept_count);
            write_text(" (+");
            write_dec(sb_bus_accept_count - last_bus_accept);
            write_text(") ");

            write_text("bus_not4us=");
            write_dec(sb_bus_reject_not_for_us_count);
            write_text(" (+");
            write_dec(sb_bus_reject_not_for_us_count - last_bus_not_for_us);
            write_text(") ");

            write_text("bus_inv=");
            write_dec(sb_bus_reject_invalid_count);
            write_text(" (+");
            write_dec(sb_bus_reject_invalid_count - last_bus_invalid);
            write_text(") ");

            write_text("bus_busy=");
            write_dec(sb_bus_reject_busy_count);
            write_text(" (+");
            write_dec(sb_bus_reject_busy_count - last_bus_busy);
            write_text(") ");

            write_text("bus_ackwait=");
            write_dec(sb_bus_ack_wait_count);
            write_text(" (+");
            write_dec(sb_bus_ack_wait_count - last_bus_ack_wait);
            write_text(")\r\n");

            write_text("err_chk=");
            write_dec(sb_bus_rxerr_checksum_count);
            write_text(" (+");
            write_dec(sb_bus_rxerr_checksum_count - last_err_checksum);
            write_text(") ");

            write_text("err_par=");
            write_dec(sb_bus_rxerr_parity_count);
            write_text(" (+");
            write_dec(sb_bus_rxerr_parity_count - last_err_parity);
            write_text(") ");

            write_text("err_stop=");
            write_dec(sb_bus_rxerr_stopbit_count);
            write_text(" (+");
            write_dec(sb_bus_rxerr_stopbit_count - last_err_stop);
            write_text(") ");

            write_text("err_timing=");
            write_dec(sb_bus_rxerr_timing_count);
            write_text(" (+");
            write_dec(sb_bus_rxerr_timing_count - last_err_timing);
            write_text(") ");

            write_text("err_len=");
            write_dec(sb_bus_rxerr_length_count);
            write_text(" (+");
            write_dec(sb_bus_rxerr_length_count - last_err_length);
            write_text(") ");

            write_text("err_pre=");
            write_dec(sb_bus_rxerr_preamble_count);
            write_text(" (+");
            write_dec(sb_bus_rxerr_preamble_count - last_err_preamble);
            write_text(") ");

            write_text("err_inv=");
            write_dec(sb_bus_rxerr_invalid_tel_count);
            write_text(" (+");
            write_dec(sb_bus_rxerr_invalid_tel_count - last_err_invalid);
            write_text(") ");

            write_text("err_busy=");
            write_dec(sb_bus_rxerr_buffer_busy_count);
            write_text(" (+");
            write_dec(sb_bus_rxerr_buffer_busy_count - last_err_busy);
            write_text(") ");

            write_text("last_mask=0x");
            write_hex32(sb_bus_rxerr_last_mask);
            write_text("\r\n");

            last_irq = sb_rx_irq_count;
            last_cap = sb_rx_capture_count;
            last_rej = sb_rx_reject_short_count;
            last_pwm = sb_tx_pwm_fire_count;
            last_time = sb_tx_time_fire_count;

            last_bus_handle = sb_bus_handle_count;
            last_bus_valid = sb_bus_valid_frame_count;
            last_bus_process = sb_bus_process_tel_count;
            last_bus_accept = sb_bus_accept_count;
            last_bus_not_for_us = sb_bus_reject_not_for_us_count;
            last_bus_invalid = sb_bus_reject_invalid_count;
            last_bus_busy = sb_bus_reject_busy_count;
            last_bus_ack_wait = sb_bus_ack_wait_count;

            last_err_checksum = sb_bus_rxerr_checksum_count;
            last_err_parity = sb_bus_rxerr_parity_count;
            last_err_stop = sb_bus_rxerr_stopbit_count;
            last_err_timing = sb_bus_rxerr_timing_count;
            last_err_length = sb_bus_rxerr_length_count;
            last_err_preamble = sb_bus_rxerr_preamble_count;
            last_err_invalid = sb_bus_rxerr_invalid_tel_count;
            last_err_busy = sb_bus_rxerr_buffer_busy_count;
        }
    }
}