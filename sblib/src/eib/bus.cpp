/**
 * bus.cpp - Low level EIB bus access.
 *
 * Copyright (c) 2014 Stefan Taferner
 * Copyright (c) 2021 Horst Rauch
 */

#include <sblib/eib/bus.h>
#include <sblib/eib/knx_lpdu.h>
#include <sblib/eib/knx_npdu.h>
#include <sblib/core.h>
#include <sblib/interrupt.h>
#include <sblib/platform.h>
#include <sblib/eib/addr_tables.h>
#include <sblib/eib/bcu_base.h>
#include <sblib/eib/bus_const.h>
#include <sblib/eib/bus_debug.h>

/*
 * Diagnostic counters for RP2354 bring-up.
 * extern in main.cpp if needed.
 */
volatile unsigned int sb_bus_handle_count = 0;
volatile unsigned int sb_bus_valid_frame_count = 0;
volatile unsigned int sb_bus_process_tel_count = 0;
volatile unsigned int sb_bus_accept_count = 0;
volatile unsigned int sb_bus_reject_not_for_us_count = 0;
volatile unsigned int sb_bus_reject_invalid_count = 0;
volatile unsigned int sb_bus_reject_busy_count = 0;
volatile unsigned int sb_bus_ack_wait_count = 0;

/*
 * Detailed RX error counters
 */
volatile unsigned int sb_bus_rxerr_checksum_count = 0;
volatile unsigned int sb_bus_rxerr_parity_count = 0;
volatile unsigned int sb_bus_rxerr_stopbit_count = 0;
volatile unsigned int sb_bus_rxerr_timing_count = 0;
volatile unsigned int sb_bus_rxerr_length_count = 0;
volatile unsigned int sb_bus_rxerr_preamble_count = 0;
volatile unsigned int sb_bus_rxerr_invalid_tel_count = 0;
volatile unsigned int sb_bus_rxerr_buffer_busy_count = 0;
volatile unsigned int sb_bus_rxerr_last_mask = 0;

// constructor for Bus object. Initialize basic interface parameter to bus and set SM to IDLE
Bus::Bus(BcuBase* bcuInstance, Timer& aTimer, int aRxPin, int aTxPin, TimerCapture aCaptureChannel, TimerMatch aPwmChannel)
:bcu(bcuInstance)
,timer(aTimer)
,rxPin(aRxPin)
,txPin(aTxPin)
,captureChannel(aCaptureChannel)
,pwmChannel(aPwmChannel)
{
    timeChannel = (TimerMatch) ((pwmChannel + 2) & 3);  // +2 to be compatible to old code during refactoring
    state = Bus::INIT;
    sendRetriesMax = NACK_RETRY_DEFAULT;
    sendBusyRetriesMax = BUSY_RETRY_DEFAULT;
    setKNX_TX_Pin(txPin);
    telegram = new byte[bcu->maxTelegramSize()]();
}

void Bus::begin()
{
    telegramLen = 0;
    rx_error = RX_OK;

    tx_error = TX_OK;
    sendCurTelegram = nullptr;
    prepareForSending();
    timer.setIRQPriority(0);
    timer.begin();
    timer.pwmEnable(pwmChannel);
    timer.start();
    timer.prescaler(TIMER_PRESCALER);
    initState();

    timer.value(0xffff);
    while (timer.getMatchChannelLevel(pwmChannel) == true);
    pinMode(txPin, OUTPUT_MATCH);
    pinMode(rxPin, INPUT_CAPTURE | HYSTERESIS);

    timer.resetFlags();
    timer.interrupts();

    DB_TELEGRAM(serial.println("DUMP_TELEGRAMS Bus telegram dump enabled."));
#ifdef DEBUG_BUS
    IF_DEBUG(serial.println("DEBUG_BUS dump enabled."));
#endif

#ifdef DEBUG_BUS_BITLEVEL
    IF_DEBUG(serial.println("DEBUG_BUS_BITLEVEL dump enabled."));
#endif

    DB_BUS(
        ttimer.begin();
        ttimer.start();
        ttimer.noInterrupts();
        ttimer.restart();
        ttimer.prescaler(TIMER_PRESCALER);
        serial.print("Bus begin - Timer prescaler: ", (unsigned int)TIMER_PRESCALER, DEC, 6);
        serial.print(" ttimer prescaler: ", ttimer.prescaler(), DEC, 6);
        serial.println(" ttimer value: ", ttimer.value(), DEC, 6);
        serial.print("nak retries: ", sendRetriesMax, DEC, 6);
        serial.print(" busy retries: ", sendBusyRetriesMax, DEC, 6);
        serial.print(" phy addr: ", PHY_ADDR_AREA(bcu->ownAddress()), DEC);
        serial.print(".", PHY_ADDR_LINE(bcu->ownAddress()), DEC);
        serial.print(".", PHY_ADDR_DEVICE(bcu->ownAddress()), DEC);
        serial.print(" (0x",  bcu->ownAddress(), HEX, 4);
        serial.println(")");
    );

#ifdef PIO_FOR_TEL_END_IND
    pinMode(PIO_FOR_TEL_END_IND, OUTPUT);
    digitalWrite(PIO_FOR_TEL_END_IND, 0);
#endif
}

void Bus::pause(bool waitForTelegramSent)
{
    auto paused = false;

    while (!paused)
    {
        noInterrupts();

        if (canPause(waitForTelegramSent))
        {
            timer.captureMode(captureChannel, FALLING_EDGE);
            timer.matchMode(timeChannel, RESET);
            timer.match(timeChannel, 0xfffe);
            state = INIT;
            paused = true;
        }

        interrupts();
        waitForInterrupt();
    }
}

void Bus::resume()
{
    noInterrupts();
    initState();
    interrupts();
}

bool Bus::canPause(bool waitForTelegramSent)
{
    if (state == IDLE)
        return true;

    if (state != WAIT_50BT_FOR_NEXT_RX_OR_PENDING_TX_OR_IDLE)
        return false;

    if (sendCurTelegram != nullptr)
    {
        if (repeatTelegram)
            return false;

        return !waitForTelegramSent;
    }

    return true;
}

void Bus::prepareTelegram(unsigned char* telegram, unsigned short length) const
{
    setSenderAddress(telegram, (uint16_t)bcu->ownAddress());

    unsigned char checksum = 0xff;
    for (unsigned short i = 0; i < length; ++i)
    {
        checksum ^= telegram[i];
    }
    telegram[length] = checksum;
}

void Bus::sendTelegram(unsigned char* telegram, unsigned short length)
{
    prepareTelegram(telegram, length);

    while (sendCurTelegram != nullptr);

    sendCurTelegram = telegram;

    DB_TELEGRAM(
        unsigned int t;
        t = ttimer.value();
        serial.print("QUE: (", t, DEC, 8);
        serial.print(") ");
        for (int i = 0; i <= length; ++i)
        {
            if (i) serial.print(" ");
            serial.print(telegram[i], HEX, 2);
        }
        serial.println();
    );

    noInterrupts();
    if (state == IDLE)
    {
        startSendingImmediately();
    }
    interrupts();
}

void Bus::initState()
{
    const uint16_t waitTime = BIT_TIMES_DELAY(2) + WAIT_40BIT;

    timer.captureMode(captureChannel, FALLING_EDGE | INTERRUPT);
    timer.matchMode(timeChannel, INTERRUPT);
    timer.restart();
    timer.match(timeChannel, waitTime);
    timer.match(pwmChannel, 0xffff);
    state = INIT;
    sendAck = 0;
}

void Bus::idleState()
{
    tb_t( 99, ttimer.value(), tb_in);
    tb_h( 99, sendAck, tb_in);

    timer.captureMode(captureChannel, FALLING_EDGE | INTERRUPT );
    timer.matchMode(timeChannel, RESET);
    timer.match(timeChannel, 0xfffe);
    timer.match(pwmChannel, 0xffff);
    state = Bus::IDLE;
}

void Bus::startSendingImmediately()
{
    state = Bus::WAIT_50BT_FOR_NEXT_RX_OR_PENDING_TX_OR_IDLE;
    timer.restart();
    timer.match(timeChannel, 1);
    timer.matchMode(timeChannel, INTERRUPT | RESET);
}

void Bus::prepareForSending()
{
    tx_error = TX_OK;

    collisions = 0;
    sendRetries = 0;
    sendBusyRetries = 0;
    sendTelegramLen = 0;
    wait_for_ack_from_remote = false;
    repeatTelegram = false;
    busy_wait_from_remote = false;
}

void Bus::handleTelegram(bool valid)
{
    ++sb_bus_handle_count;

#ifdef DEBUG_BUS
    b1 = (((unsigned int)rx_telegram[0]<<24) |((unsigned int)rx_telegram[1]<<16) |((unsigned int)rx_telegram[2]<<8)| (rx_telegram[3]));
    b2= ( ((unsigned int)rx_telegram[4]<<8) | (rx_telegram[5]));
    b3= (( unsigned int)rx_telegram[6]<<8)|((unsigned int)rx_telegram[7]);
    b4= nextByteIndex;
    b5= ( collisions + (valid ? 8 : 0));
    tb2( 9000, b5,  b1, b2, b3, b4, tb_in);
#endif

    DB_TELEGRAM(
        if (nextByteIndex){
            for (int i = 0; i < nextByteIndex; ++i)
            {
                telBuffer[i] = rx_telegram[i];
            }
            telLength = nextByteIndex;
            telcollisions = collisions;
        }
    );

    sendAck = 0;
    int time = SEND_WAIT_TIME -  PRE_SEND_TIME;
    state = Bus::WAIT_50BT_FOR_NEXT_RX_OR_PENDING_TX_OR_IDLE;
    tb_h( 908, currentByte, tb_in);
    tb_h( 909, parity, tb_in);

#ifndef BUSMONITOR

    if (nextByteIndex >= 8 && valid && (( rx_telegram[0] & VALID_DATA_FRAME_TYPE_MASK) == VALID_DATA_FRAME_TYPE_VALUE)
        && nextByteIndex <= bcu->maxTelegramSize()  )
    {
        ++sb_bus_valid_frame_count;

        int destAddr = (rx_telegram[3] << 8) | rx_telegram[4];
        bool processTel = false;

        if (rx_telegram[5] & 0x80)
        {
            processTel = (destAddr == 0);
            processTel |= (bcu->addrTables != nullptr) && (bcu->addrTables->indexOfAddr(destAddr) >= 0);
        }
        else if (destAddr == bcu->ownAddress())
        {
            processTel = true;
        }

        processTel |= !(bcu->userRam->status() & BCU_STATUS_TRANSPORT_LAYER);

        DB_TELEGRAM(telRXNotProcessed = !processTel);

        if (processTel)
        {
            ++sb_bus_process_tel_count;

            bool already_received = false;
            if (!(rx_telegram[0] & SB_TEL_REPEAT_FLAG))
            {
                if ((rx_telegram[0] & ~SB_TEL_REPEAT_FLAG) == (telegram[0] & ~SB_TEL_REPEAT_FLAG))
                {
                    int i;
                    for (i = 1; (i < nextByteIndex - 1) && (rx_telegram[i] == telegram[i]); i++);
                    if (i == nextByteIndex - 1) {
                        already_received = true;
                    }
                }
            }

            if (telegramLen)
            {
                ++sb_bus_reject_busy_count;
                sendAck = 0;
                rx_error |= RX_BUFFER_BUSY;
            }
            else
            {
                sendAck = SB_BUS_ACK;
                if (!already_received)
                {
                    for (int i = 0; i < nextByteIndex; i++) telegram[i] = rx_telegram[i];
                    telegramLen = nextByteIndex;
                    rx_error = RX_OK;
                    ++sb_bus_accept_count;
                }
            }

            auto suppressAck = !(bcu->userRam->status() & BCU_STATUS_LINK_LAYER);
            suppressAck |= rx_telegram[0] & SB_TEL_DATA_FRAME_FLAG;
            if (suppressAck)
            {
                sendAck = 0;
            }

            if (sendAck)
            {
                ++sb_bus_ack_wait_count;
                state = Bus::RECV_WAIT_FOR_ACK_TX_START;
                time = SEND_ACK_WAIT_TIME - PRE_SEND_TIME;
            }
        }
        else
        {
            ++sb_bus_reject_not_for_us_count;
        }
    }
    else if (nextByteIndex == 1 && wait_for_ack_from_remote)
    {
        tb_h( 907, currentByte, tb_in);

        wait_for_ack_from_remote = false;
        rx_error &= ~RX_CHECKSUM_ERROR;

        if ((parity && currentByte == SB_BUS_ACK) || sendRetries >= sendRetriesMax || sendBusyRetries >= sendBusyRetriesMax)
        {
            if (!(parity && currentByte == SB_BUS_ACK))
                tx_error |= TX_RETRY_ERROR;
            tb_h( 906, tx_error, tb_in);
            finishSendingTelegram();
        }
        else if (parity && (currentByte == SB_BUS_BUSY || currentByte == SB_BUS_NACK_BUSY))
        {
            time = BUSY_WAIT_150BIT - PRE_SEND_TIME;
            tx_error |= TX_REMOTE_BUSY_ERROR;
            busy_wait_from_remote = true;
            repeatTelegram = true;
        }
        else
        {
            tx_error |= TX_NACK_ERROR;
            busy_wait_from_remote = false;
            repeatTelegram = true;
        }
    }
    else
    {
        ++sb_bus_reject_invalid_count;

        auto isAcknowledgeFrame = (nextByteIndex == 1) &&
                                  (currentByte == SB_BUS_ACK ||
                                   currentByte == SB_BUS_NACK ||
                                   currentByte == SB_BUS_BUSY ||
                                   currentByte == SB_BUS_NACK_BUSY);
        if (isAcknowledgeFrame)
        {
            rx_error &= ~RX_INVALID_TELEGRAM_ERROR;
            rx_error &= ~RX_CHECKSUM_ERROR;
        }
        else
        {
            rx_error |= RX_INVALID_TELEGRAM_ERROR;
        }
    }

    if (wait_for_ack_from_remote)
    {
        repeatTelegram = true;
        wait_for_ack_from_remote = false;
    }

    if (rx_error & RX_CHECKSUM_ERROR)        ++sb_bus_rxerr_checksum_count;
    if (rx_error & RX_PARITY_ERROR)          ++sb_bus_rxerr_parity_count;
    if (rx_error & RX_STOPBIT_ERROR)         ++sb_bus_rxerr_stopbit_count;
    if (rx_error & RX_TIMING_ERROR_SPIKE)    ++sb_bus_rxerr_timing_count;
    if (rx_error & RX_LENGTH_ERROR)          ++sb_bus_rxerr_length_count;
    if (rx_error & RX_PREAMBLE_ERROR)        ++sb_bus_rxerr_preamble_count;
    if (rx_error & RX_INVALID_TELEGRAM_ERROR)++sb_bus_rxerr_invalid_tel_count;
    if (rx_error & RX_BUFFER_BUSY)           ++sb_bus_rxerr_buffer_busy_count;
    sb_bus_rxerr_last_mask = rx_error;

    DB_TELEGRAM(telrxerror = rx_error);

    tb_d( 901, state, tb_in);    tb_d( 902, sendRetries, tb_in);   tb_d( 903, sendBusyRetries, tb_in); tb_h( 904, sendAck, tb_in);
    tb_h( 905, rx_error, tb_in); tb_d( 910, telegramLen, tb_in); tb_d( 911, nextByteIndex, tb_in);

#endif
#ifdef BUSMONITOR
    rx_error = RX_OK;
#endif

    timer.captureMode(captureChannel, FALLING_EDGE | INTERRUPT );
    timer.match(timeChannel, time - 1);
}

void Bus::finishSendingTelegram()
{
    if (sendCurTelegram != nullptr)
    {
        sendCurTelegram = nullptr;
        bcu->finishedSendingTelegram(!(tx_error & TX_RETRY_ERROR));
    }

    prepareForSending();
}

void Bus::encounteredCollision()
{
    if (!sendAck)
    {
        collisions++;
        tx_error |= TX_COLLISION_ERROR;
    }
}

/*
 * State Machine - driven by interrupts of timer and capture input
 */
__attribute__((optimize("Os"))) void Bus::timerInterruptHandler()
{
    bool timeout;
    int time;
    unsigned int dt, tv, cv;
    auto isCaptureEvent = timer.flag(captureChannel);

    tbint(state+8000, ttimer.value(), isCaptureEvent, timer.capture(captureChannel), timer.value(), timer.match(timeChannel), tb_in);

    if (isCaptureEvent)
    {
        auto captureValue = timer.capture(captureChannel);
        auto matchValue = timer.match(timeChannel);

        if (captureValue < timer.match(pwmChannel))
        {
            while (true)
            {
                if (digitalRead(rxPin))
                {
                    timer.resetFlag(captureChannel);
                    return;
                }

                auto timerValue = timer.value();
                auto elapsedMicroseconds = (timerValue >= captureValue) ? (timerValue - captureValue) : (matchValue + 1 - captureValue + timerValue);
                if (elapsedMicroseconds > ZERO_BIT_MIN_TIME)
                {
                    break;
                }
            }
        }
    }

    STATE_SWITCH:
    switch (state)
    {
    case Bus::INIT:
        tb_t( state, ttimer.value(), tb_in);
        DB_TELEGRAM(telRXWaitInitTime = ttimer.value());

        if (!timer.flag(timeChannel))
        {
            timer.value(ZERO_BIT_MIN_TIME + 2);
            break;
        }

        timer.match(timeChannel, BIT_TIMES_DELAY(2) + WAIT_50BIT_FOR_IDLE - PRE_SEND_TIME);
        timer.matchMode(timeChannel, INTERRUPT | RESET);
        state = WAIT_50BT_FOR_NEXT_RX_OR_PENDING_TX_OR_IDLE;
        if (timer.flag(captureChannel))
            goto STATE_SWITCH;
        break;

    case Bus::IDLE:
        tb_d( state+100, ttimer.value(), tb_in);
        DB_TELEGRAM(telRXWaitIdleTime = ttimer.value());

        if (!isCaptureEvent)
            break;

    case Bus::INIT_RX_FOR_RECEIVING_NEW_TEL:
        tb_d( state+100, ttimer.value(), tb_in);

        DB_TELEGRAM(
            tv=timer.value(); cv= timer.capture(captureChannel);
            if ( tv > cv ) dt= tv - cv;
            else dt = (timer.match(timeChannel) + 1 - cv) + tv;

            telRXStartTime= ttimer.value()- dt;
        );

        nextByteIndex = 0;
        rx_error  = RX_OK;
        checksum = 0xff;
        sendAck = 0;
        valid = 1;

    case Bus:: RECV_WAIT_FOR_STARTBIT_OR_TELEND:
        time = timer.match(timeChannel);
        timer.match(timeChannel, 0xfffe);
        timer.matchMode(timeChannel, INTERRUPT | RESET);

        if (!isCaptureEvent)
        {
            if (checksum)
            {
                rx_error |= RX_CHECKSUM_ERROR;
            }
            DB_TELEGRAM(telRXEndTime = telRXTelByteEndTime);
#           ifdef PIO_FOR_TEL_END_IND
                digitalWrite(PIO_FOR_TEL_END_IND, 1);
#           endif
            handleTelegram(valid && !checksum);
            break;
        }

        tv=timer.value(); cv= timer.capture(captureChannel);
        if ( tv >= cv ) dt= tv - cv;
        else dt = (time + 1 - cv) + tv;
        timer.value(dt+2);
        timer.match(timeChannel, BYTE_TIME_INCL_STOP - 1);
        timer.captureMode(captureChannel, FALLING_EDGE | INTERRUPT);
        state = Bus::RECV_BITS_OF_BYTE;
        currentByte = 0;
        bitTime = 0;
        bitMask = 1;
        parity = 1;

        DB_TELEGRAM(telRXTelByteStartTime = ttimer.value() - dt);
        break;

    case Bus::RECV_BITS_OF_BYTE:
        timeout = timer.flag(timeChannel);
        if (timeout) time = timer.match(timeChannel) + 1;
        else
        {
            time = timer.capture(captureChannel);
        }

        if (time >= bitTime + BIT_TIME - 35 )
        {
            bitTime += BIT_TIME;
            while (time >= bitTime + BIT_WAIT_TIME && bitMask <= 0x100)
            {
                currentByte |= bitMask;
                parity = !parity;
                bitTime += BIT_TIME;
                bitMask <<= 1;
            }

            if (time > bitTime + BIT_OFFSET_MAX && bitMask <= 0x100)
            {
                rx_error |= RX_TIMING_ERROR_SPIKE;
                DB_TELEGRAM(telRXTelBitTimingErrorLate = time);
            }
            bitMask <<= 1;
        }
        else
        {
            rx_error |= RX_TIMING_ERROR_SPIKE;
            DB_TELEGRAM(telRXTelBitTimingErrorEarly = time);
        }

        if (timeout)
        {
            DB_TELEGRAM(telRXTelByteEndTime = ttimer.value() - timer.value());

            currentByte &= 0xff;

            if ( (!nextByteIndex) && (currentByte & PREAMBLE_MASK) )
                rx_error |= RX_PREAMBLE_ERROR;

            if (nextByteIndex < bcu->maxTelegramSize())
            {
                rx_telegram[nextByteIndex++] = currentByte;
                checksum ^= currentByte;
            }
            else
            {
                rx_error |= RX_LENGTH_ERROR;
            }

            if (!parity) rx_error |= RX_PARITY_ERROR;
            valid &= parity;
            tb_h( RECV_BITS_OF_BYTE +300, currentByte, tb_in);

            state = Bus:: RECV_WAIT_FOR_STARTBIT_OR_TELEND;
            timer.match(timeChannel, MAX_INTER_CHAR_TIME - 1);
            timer.matchMode(timeChannel, INTERRUPT);
            timer.captureMode(captureChannel, FALLING_EDGE | INTERRUPT );

        }
        else if (time > BYTE_TIME_EXCL_STOP )
            rx_error |= RX_STOPBIT_ERROR;
        break;

    case Bus::RECV_WAIT_FOR_ACK_TX_START:
        tb_t( state, ttimer.value(), tb_in);

        if (isCaptureEvent)
        {
            sendAck = 0;
            state = Bus::INIT_RX_FOR_RECEIVING_NEW_TEL;
            goto STATE_SWITCH;
        }
        sendTelegramLen = 0;

        DB_TELEGRAM(
            telTXAck = sendAck;
            telTXStartTime = ttimer.value() + PRE_SEND_TIME;
        );

        timer.match(pwmChannel, PRE_SEND_TIME);
        timer.match(timeChannel, PRE_SEND_TIME  + BIT_PULSE_TIME - 1);
        timer.matchMode(timeChannel, RESET | INTERRUPT);
        timer.captureMode(captureChannel, FALLING_EDGE | INTERRUPT );
        nextByteIndex = 0;
        tx_error = TX_OK;
        state = Bus::SEND_START_BIT;

        break;

    case Bus::WAIT_50BT_FOR_NEXT_RX_OR_PENDING_TX_OR_IDLE:
        tb_t( state, ttimer.value(), tb_in);

        if (isCaptureEvent)
        {
            state = Bus::INIT_RX_FOR_RECEIVING_NEW_TEL;
            goto STATE_SWITCH;
        }

        if ((repeatTelegram && (sendRetries >= sendRetriesMax || sendBusyRetries >= sendBusyRetriesMax)) ||
            collisions > COLLISION_RETRY_MAX)
        {
            tb_h( state+ 100, sendRetries + 10 * sendBusyRetries + 100 * collisions, tb_in);
            tx_error |= TX_RETRY_ERROR;
            finishSendingTelegram();
        }

        if (sendCurTelegram != nullptr)
        {
            tb_h( state+ 200, repeatTelegram, tb_in);

            sendTelegramLen = telegramSize(sendCurTelegram) + 1;

            if (repeatTelegram && (sendCurTelegram[0] & SB_TEL_REPEAT_FLAG) )
            {
                tb_d( state+ 700, sendRetries, tb_in);
                tb_d( state+ 800, sendBusyRetries, tb_in);
                sendCurTelegram[0] &= ~SB_TEL_REPEAT_FLAG;
                sendCurTelegram[sendTelegramLen - 1] ^= SB_TEL_REPEAT_FLAG;
            }

            if (((sendCurTelegram[0] & SB_TEL_REPEAT_FLAG)) && ((sendCurTelegram[0] & PRIO_FLAG_HIGH)) ) {
                time = PRE_SEND_TIME + BIT_TIMES_DELAY(3);
            }
            else
                time = PRE_SEND_TIME;

            auto canRepeat = sendRetriesMax > 0 && sendBusyRetriesMax > 0;
            auto isLastRepeatChance = repeatTelegram && (sendRetries + 1 >= sendRetriesMax || sendBusyRetries + 1 >= sendBusyRetriesMax);
            auto isLastCollisionChance = collisions == COLLISION_RETRY_MAX;
            if (canRepeat && !isLastRepeatChance && !isLastCollisionChance)
            {
                time += (millis() * RANDOMIZE_FACTOR) % RANDOMIZE_MODULUS;
            }
        }
        else
        {
            DB_BUS(
               if (sendCurTelegram != nullptr)
               {
                   tb_h( state+ 900,sendCurTelegram[0], tb_in);
               }
            );

            idleState();
            break;
        }

        tb_t( state+500, ttimer.value(), tb_in);
        tb_d( state+600, time, tb_in);
        timer.match(pwmChannel, time);
        timer.match(timeChannel, time + (BIT_PULSE_TIME - 1));
        timer.matchMode(timeChannel, RESET | INTERRUPT);
        nextByteIndex = 0;
        tx_error = TX_OK;
        state = Bus::SEND_START_BIT;

        DB_TELEGRAM(telTXStartTime = ttimer.value() + time);
        break;

    case Bus::SEND_START_BIT:
        if (!timer.flag(timeChannel))
        {
            auto captureTime = timer.capture(captureChannel);
            auto pwmTime = timer.match(pwmChannel);

            if (captureTime < (pwmTime - STARTBIT_OFFSET_MIN))
            {
                if (nextByteIndex)
                {
                    encounteredCollision();
                }

                tb_d( state+300, ttimer.value(), tb_in);
                timer.match(pwmChannel, 0xffff);
                state = Bus::INIT_RX_FOR_RECEIVING_NEW_TEL;
                goto STATE_SWITCH;
            }

            if (captureTime < pwmTime)
            {
                tb_d( state+300, ttimer.value(), tb_in);
                timer.match(pwmChannel, timer.value() + 1);
                timer.match(timeChannel, captureTime + (BIT_PULSE_TIME - 1));
            }

            tb_t( state+400, ttimer.value(), tb_in);
            state = Bus::SEND_BIT_0;
#       ifdef PIO_FOR_TEL_END_IND
            if (sendAck)
                digitalWrite(PIO_FOR_TEL_END_IND, 0);
#       endif
            break;
        }
        else
        {
            tb_t( state+400, ttimer.value(), tb_in);
            state = Bus::SEND_BIT_0;
            tx_error |= TX_PWM_STARTBIT_ERROR;
        }

    case Bus::SEND_BIT_0:
        if (sendAck)
        {
            currentByte = sendAck;
        }
        else
        {
            currentByte = sendCurTelegram[nextByteIndex++];
        }

        for (bitMask = 1; bitMask < 0x100; bitMask <<= 1)
        {
            if (currentByte & bitMask)
                currentByte ^= 0x100;
        }
        bitMask = 1;
        state = Bus::SEND_BITS_OF_BYTE;
        tb_h( SEND_BIT_0 +200, currentByte, tb_in);

    case Bus::SEND_BITS_OF_BYTE:
    {
        tb_t( state, ttimer.value(), tb_in);

        if (!timer.flag(timeChannel))
        {
            auto captureTime = timer.capture(captureChannel);

            if (captureTime < REFLECTION_IGNORE_DELAY)
            {
                break;
            }

            if ((captureTime % BIT_TIME) < (BIT_WAIT_TIME - BIT_OFFSET_MIN))
            {
                if (nextByteIndex)
                {
                    encounteredCollision();
                }

                tb_d( state+400, captureTime, tb_in);
                tb_t( state+300, ttimer.value(), tb_in);

                rx_error = RX_OK;
                checksum = 0xff;
                valid = 1;
                parity = 1;

                if (sendAck)
                {
                    sendAck = 0;
                }
                else
                {
                    nextByteIndex--;

                    for (auto i = 0; i < nextByteIndex; i++)
                    {
                        auto b = sendCurTelegram[i];
                        rx_telegram[i] = b;
                        checksum ^= b;
                    }
                }

                auto collisionBitCount = (timer.match(timeChannel) - captureTime + (BIT_OFFSET_MAX - BIT_PULSE_TIME)) / BIT_TIME;
                bitMask >>= collisionBitCount + 1;
                bitTime = captureTime - BIT_TIME;
                currentByte &= (bitMask - 1);

                auto missingBits = 10;
                for (auto i = bitMask >> 1; i; i >>= 1)
                {
                    missingBits--;
                    if (currentByte & i)
                    {
                        parity = !parity;
                    }
                }

                timer.match(timeChannel, captureTime + missingBits * BIT_TIME - 1);
                timer.matchMode(timeChannel, INTERRUPT | RESET);

                timer.match(pwmChannel, 0xffff);

                state = Bus::RECV_BITS_OF_BYTE;
                goto STATE_SWITCH;
            }

            break;
        }

        if (bitMask <= 0x200)
        {
            time = BIT_TIME ;
            while ((currentByte & bitMask) && bitMask <= 0x100)
            {
                bitMask <<= 1;
                time += BIT_TIME;
            }
            bitMask <<= 1;

            auto stopBitReached = (bitMask > 0x200);

            if (stopBitReached)
                timer.match(pwmChannel, 0xffff);
            else
                timer.match(pwmChannel, time - BIT_PULSE_TIME);

            timer.match(timeChannel, time - 1);
            break;
        }

        state = Bus::SEND_END_OF_BYTE;
    }

    case Bus::SEND_END_OF_BYTE:
        tb_t( state, ttimer.value(), tb_in);
        if (nextByteIndex < sendTelegramLen && !sendAck)
        {
            time = BIT_TIMES_DELAY(3);
            state = Bus::SEND_START_BIT;
            timer.match(pwmChannel, time - BIT_PULSE_TIME);
        }
        else
        {
            state = Bus::SEND_END_OF_TX;
            time = BIT_TIME - BIT_PULSE_TIME;
            timer.captureMode(captureChannel, FALLING_EDGE);
        }
        timer.match(timeChannel, time - 1);
        break;

    case Bus::SEND_END_OF_TX:
        tb_t( state, ttimer.value(), tb_in);
        tb_h( SEND_END_OF_TX+700, repeatTelegram, tb_in);
        DB_TELEGRAM(telTXEndTime = ttimer.value());

        if (sendAck){
            tb_h( SEND_END_OF_TX+200, tx_error, tb_in);
            DB_TELEGRAM(
                txtelBuffer[0] = sendAck;
                txtelLength = 1;
                tx_rep_count = sendRetries;
                tx_busy_rep_count = sendBusyRetries;
                tx_telrxerror = tx_error;
            );

            sendAck = 0;
            state = Bus::WAIT_50BT_FOR_NEXT_RX_OR_PENDING_TX_OR_IDLE;
            time =  SEND_WAIT_TIME - PRE_SEND_TIME;
            timer.captureMode(captureChannel, FALLING_EDGE | INTERRUPT);
        }
        else
        {
            tb_h( SEND_END_OF_TX+600, repeatTelegram, tb_in);

            wait_for_ack_from_remote = true;
            time = ACK_WAIT_TIME_MIN;
            state = Bus::SEND_WAIT_FOR_RX_ACK_WINDOW;
            timer.matchMode(timeChannel, INTERRUPT);
            if (repeatTelegram)
            {
                if (busy_wait_from_remote)
                    sendBusyRetries++;
                else
                    sendRetries++;
            }

            DB_TELEGRAM(
                for (int i =0; i< sendTelegramLen; i++)
                {
                    txtelBuffer[i] = sendCurTelegram[i];
                }
                txtelLength = sendTelegramLen;
                tx_rep_count = sendRetries;
                tx_busy_rep_count = sendBusyRetries;
                tx_telrxerror = tx_error;
            );
        }
        tb_d( SEND_END_OF_TX+300, wait_for_ack_from_remote , tb_in);
        tb_d( SEND_END_OF_TX+400, sendRetries, tb_in);
        tb_d(SEND_END_OF_TX+500, sendBusyRetries, tb_in);

        timer.match(timeChannel, time - 1);
        break;

    case Bus::SEND_WAIT_FOR_RX_ACK_WINDOW:
        tb_t( state, ttimer.value(), tb_in);

        state = Bus::SEND_WAIT_FOR_RX_ACK;
        timer.captureMode(captureChannel, FALLING_EDGE | INTERRUPT );
        timer.match(timeChannel, ACK_WAIT_TIME_MAX - 1);
        break;

    case Bus::SEND_WAIT_FOR_RX_ACK:
        tb_t( state, ttimer.value(), tb_in);

        if (isCaptureEvent){
            state = Bus::INIT_RX_FOR_RECEIVING_NEW_TEL;
            goto STATE_SWITCH;
        }
        repeatTelegram = true;
        wait_for_ack_from_remote = false;
        tx_error|= TX_ACK_TIMEOUT_ERROR;
        state = Bus::WAIT_50BT_FOR_NEXT_RX_OR_PENDING_TX_OR_IDLE;

        timer.match(timeChannel, SEND_WAIT_TIME - PRE_SEND_TIME - 1);
        timer.matchMode(timeChannel, INTERRUPT | RESET);
        break;

    default:
        tb_d( 9999, ttimer.value(), tb_in);
        fatalError();
        break;
    }

    timer.resetFlags();
}

void Bus::loop()
{
    DB_TELEGRAM(dumpTelegrams());
#if defined (DEBUG_BUS) || defined (DEBUG_BUS_BITLEVEL)
    debugBus();
#endif
}