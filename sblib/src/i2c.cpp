#include <sblib/i2c.h>

#ifdef SBLIB_PLATFORM_RP2354

#include "hardware/i2c.h"
#include "hardware/gpio.h"

#define I2C_PORT i2c0
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5

bool i2c_initialized = false;

extern "C" {

void i2c_lpcopen_init(void)
{
    if (i2c_initialized)
        return;

    Chip_I2C_Init(I2C0);
    i2c_initialized = true;
}

void Chip_I2C_Init(I2C_ID_T)
{
    i2c_init(I2C_PORT, 100000);

    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

void Chip_I2C_DeInit(I2C_ID_T)
{
}

void Chip_I2C_SetClockRate(I2C_ID_T, uint32_t rate)
{
    i2c_set_baudrate(I2C_PORT, rate);
}

uint32_t Chip_I2C_GetClockRate(I2C_ID_T)
{
    return 100000;
}

int Chip_I2C_MasterSend(I2C_ID_T, uint8_t addr, const uint8_t *buff, uint8_t len)
{
    return i2c_write_blocking(I2C_PORT, addr, buff, len, false);
}

int Chip_I2C_MasterRead(I2C_ID_T, uint8_t addr, uint8_t *buff, int len)
{
    return i2c_read_blocking(I2C_PORT, addr, buff, len, false);
}

int Chip_I2C_MasterCmdRead(I2C_ID_T, uint8_t addr, uint8_t cmd, uint8_t *buff, int len)
{
    int r = i2c_write_blocking(I2C_PORT, addr, &cmd, 1, true);
    if (r < 0)
        return r;

    return i2c_read_blocking(I2C_PORT, addr, buff, len, false);
}

int Chip_I2C_MasterWriteRead(I2C_ID_T, uint8_t addr, uint8_t *cmd,
                             uint8_t *buff, int txlen, int rxlen)
{
    int r = i2c_write_blocking(I2C_PORT, addr, cmd, txlen, true);
    if (r < 0)
        return r;

    return i2c_read_blocking(I2C_PORT, addr, buff, rxlen, false);
}

void Chip_I2C_EventHandlerPolling(I2C_ID_T, I2C_EVENT_T) {}
void Chip_I2C_EventHandler(I2C_ID_T, I2C_EVENT_T) {}
void Chip_I2C_MasterStateHandler(I2C_ID_T) {}
void Chip_I2C_Disable(I2C_ID_T) {}
int Chip_I2C_IsMasterActive(I2C_ID_T) { return 0; }
void Chip_I2C_SlaveSetup(I2C_ID_T, I2C_SLAVE_ID, I2C_XFER_T*, I2C_EVENTHANDLER_T, uint8_t) {}
void Chip_I2C_SlaveStateHandler(I2C_ID_T) {}
int Chip_I2C_IsStateChanged(I2C_ID_T) { return 0; }
int Chip_I2C_SetMasterEventHandler(I2C_ID_T, I2C_EVENTHANDLER_T) { return 1; }
I2C_EVENTHANDLER_T Chip_I2C_GetMasterEventHandler(I2C_ID_T) { return 0; }
int Chip_I2C_MasterTransfer(I2C_ID_T, I2C_XFER_T *xfer)
{
    if (!xfer)
        return I2C_STATUS_BUSERR;

    if (xfer->txSz > 0 && xfer->rxSz > 0)
    {
        int r = Chip_I2C_MasterWriteRead(I2C0, xfer->slaveAddr,
                                         const_cast<uint8_t*>(xfer->txBuff),
                                         xfer->rxBuff,
                                         xfer->txSz, xfer->rxSz);
        xfer->status = (r < 0) ? I2C_STATUS_NAK : I2C_STATUS_DONE;
        return xfer->status;
    }

    if (xfer->txSz > 0)
    {
        int r = Chip_I2C_MasterSend(I2C0, xfer->slaveAddr, xfer->txBuff, (uint8_t)xfer->txSz);
        xfer->status = (r < 0) ? I2C_STATUS_NAK : I2C_STATUS_DONE;
        return xfer->status;
    }

    if (xfer->rxSz > 0)
    {
        int r = Chip_I2C_MasterRead(I2C0, xfer->slaveAddr, xfer->rxBuff, xfer->rxSz);
        xfer->status = (r < 0) ? I2C_STATUS_NAK : I2C_STATUS_DONE;
        return xfer->status;
    }

    xfer->status = I2C_STATUS_DONE;
    return xfer->status;
}

} // extern "C"

#endif