/*
 * @brief I2C abstraction for Selfbus
 *
 * RP2354 port:
 * - keeps the public Selfbus/LPCOpen-style API surface that higher layers use
 * - removes LPC register definitions and CMSIS-specific __IO/__I/__O types
 */

#ifndef __I2C_11XX_H_
#define __I2C_11XX_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I2C Slave Identifiers
 */
typedef enum {
    I2C_SLAVE_GENERAL,
    I2C_SLAVE_0,
    I2C_SLAVE_1,
    I2C_SLAVE_2,
    I2C_SLAVE_3,
    I2C_SLAVE_NUM_INTERFACE
} I2C_SLAVE_ID;

/**
 * @brief I2C transfer status
 */
typedef enum {
    I2C_STATUS_DONE,
    I2C_STATUS_NAK,
    I2C_STATUS_ARBLOST,
    I2C_STATUS_BUSERR,
    I2C_STATUS_BUSY,
} I2C_STATUS_T;

/**
 * @brief Master transfer data structure definitions
 */
typedef struct {
    uint8_t slaveAddr;        /**< 7-bit I2C slave address */
    const uint8_t *txBuff;    /**< Pointer to bytes to transmit */
    int txSz;                 /**< Number of bytes to transmit */
    uint8_t *rxBuff;          /**< Pointer to receive buffer */
    int rxSz;                 /**< Number of bytes to receive */
    I2C_STATUS_T status;      /**< Status of the current I2C transfer */
} I2C_XFER_T;

/**
 * @brief I2C interface IDs
 */
typedef enum I2C_ID {
    I2C0,
    I2C_NUM_INTERFACE
} I2C_ID_T;

/**
 * @brief I2C master/slave events
 */
typedef enum {
    I2C_EVENT_WAIT = 1,
    I2C_EVENT_DONE,
    I2C_EVENT_LOCK,
    I2C_EVENT_UNLOCK,
    I2C_EVENT_SLAVE_RX,
    I2C_EVENT_SLAVE_TX,
} I2C_EVENT_T;

/**
 * @brief Event handler function type
 */
typedef void (*I2C_EVENTHANDLER_T)(I2C_ID_T, I2C_EVENT_T);

extern bool i2c_initialized;

/**
 * @brief Initializes the default I2C bus/pins.
 */
void i2c_lpcopen_init(void);

/**
 * @brief Initializes the I2C peripheral.
 */
void Chip_I2C_Init(I2C_ID_T id);

/**
 * @brief De-initializes the I2C peripheral.
 */
void Chip_I2C_DeInit(I2C_ID_T id);

/**
 * @brief Set I2C clock rate.
 */
void Chip_I2C_SetClockRate(I2C_ID_T id, uint32_t clockrate);

/**
 * @brief Get current I2C clock rate.
 */
uint32_t Chip_I2C_GetClockRate(I2C_ID_T id);

/**
 * @brief Transmit and receive data in master mode.
 */
int Chip_I2C_MasterTransfer(I2C_ID_T id, I2C_XFER_T *xfer);

/**
 * @brief Master write only.
 */
int Chip_I2C_MasterSend(I2C_ID_T id, uint8_t slaveAddr, const uint8_t *buff, uint8_t len);

/**
 * @brief Master write one-byte command then repeated-start read.
 */
int Chip_I2C_MasterCmdRead(I2C_ID_T id, uint8_t slaveAddr, uint8_t cmd, uint8_t *buff, int len);

/**
 * @brief Master write then repeated-start read.
 */
int Chip_I2C_MasterWriteRead(I2C_ID_T id, uint8_t slaveAddr, uint8_t *cmd, uint8_t *buff, int txlen, int rxlen);

/**
 * @brief Master read only.
 */
int Chip_I2C_MasterRead(I2C_ID_T id, uint8_t slaveAddr, uint8_t *buff, int len);

/**
 * @brief Optional compatibility declarations.
 */
void Chip_I2C_EventHandlerPolling(I2C_ID_T id, I2C_EVENT_T event);
void Chip_I2C_EventHandler(I2C_ID_T id, I2C_EVENT_T event);
void Chip_I2C_MasterStateHandler(I2C_ID_T id);
void Chip_I2C_Disable(I2C_ID_T id);
int Chip_I2C_IsMasterActive(I2C_ID_T id);
void Chip_I2C_SlaveSetup(I2C_ID_T id,
                         I2C_SLAVE_ID sid,
                         I2C_XFER_T *xfer,
                         I2C_EVENTHANDLER_T event,
                         uint8_t addrMask);
void Chip_I2C_SlaveStateHandler(I2C_ID_T id);
int Chip_I2C_IsStateChanged(I2C_ID_T id);
int Chip_I2C_SetMasterEventHandler(I2C_ID_T id, I2C_EVENTHANDLER_T event);
I2C_EVENTHANDLER_T Chip_I2C_GetMasterEventHandler(I2C_ID_T id);

#ifdef __cplusplus
}
#endif

#endif /* __I2C_11XX_H_ */