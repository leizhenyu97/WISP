// AD7124 register/SPI implementation for the firmware acquisition path.
// This file handles shadow-register transactions, readiness polling, setup/reset, and SPI target dispatch.

#include <stdint.h>

#include "Communication.h"
#include "AD7124.h"
#include "ad7124_regs.h"
ad7124_st_reg *regs = ad7124_regs; // Points to the active software shadow-register table.
uint8_t useCRC;                    // Selects whether transfer CRC bytes are generated and checked.
int check_ready;                   // Enables SPI-ready polling based on the configured error bits.
int spi_rdy_poll_cnt;              // Default polling budget used before SPI transactions.
	
/**
 * Initializes the driver globals and default runtime settings.
 *
 * @return None.
 */
void AD7124_Init()
{
    regs = ad7124_regs;
    check_ready = 0;
    useCRC = AD7124_DISABLE_CRC;
    spi_rdy_poll_cnt = 25000;
}

/**
 * Reads one register without SPI-ready polling.
 *
 * @param pReg Pointer to the register descriptor to update.
 * @return 0 on success or a negative error code.
 */
int32_t AD7124_NoCheckReadRegister(ad7124_st_reg* pReg)
{
    int32_t ret       = 0;
    uint8_t _buffer[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t i         = 0;
    uint8_t check8    = 0;
    uint8_t msgBuf[8] = {0, 0, 0, 0, 0, 0, 0, 0};


    check8 = useCRC;

    /* Build the AD7124 communication byte: WEN | RD | register address. */
    _buffer[0] = AD7124_COMM_REG_WEN | AD7124_COMM_REG_RD |
                AD7124_COMM_REG_RA(pReg->addr);

    /* Read the command byte plus the register payload and optional CRC byte. */
    ret = AD7124_SPI_Read(_buffer,
                   ((useCRC != AD7124_DISABLE_CRC) ? pReg->_size + 1
                    : pReg->_size) + 1);
    if(ret < 0)
        return ret;

    /* Rebuild the command+payload stream because the AD7124 CRC covers both. */
    if(check8 == AD7124_USE_CRC) {
        msgBuf[0] = AD7124_COMM_REG_WEN | AD7124_COMM_REG_RD |
                    AD7124_COMM_REG_RA(pReg->addr);
        for(i = 1; i < pReg->_size + 2; ++i) {
            msgBuf[i] = _buffer[i];
        }
        check8 = AD7124_ComputeCRC8(msgBuf, pReg->_size + 2);
    }

    if(check8 != 0) {
        /* ReadRegister checksum failed. */
        return COMM_ERR;
    }

    /* Fold the returned register bytes into the shadow value MSB-first. */
    pReg->value = 0;
    for(i = 1; i < pReg->_size + 1; i++) {
        pReg->value <<= 8;
        pReg->value += _buffer[i];
    }

    return ret;
}

/**
 * Writes one register without SPI-ready polling.
 *
 * @param reg Register descriptor containing the value to write.
 * @return 0 on success or a negative error code.
 */
int32_t AD7124_NoCheckWriteRegister(ad7124_st_reg reg)
{
    int32_t ret      = 0;
    int32_t regValue = 0;
    uint8_t wrBuf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t i        = 0;
    uint8_t crc8     = 0;


    /* Build the AD7124 communication byte: WEN | WR | register address. */
    wrBuf[0] = AD7124_COMM_REG_WEN | AD7124_COMM_REG_WR |
               AD7124_COMM_REG_RA(reg.addr);

    /* Emit the register payload MSB-first after the communication byte. */
    regValue = reg.value;
    for(i = 0; i < reg._size; i++) {
        wrBuf[reg._size - i] = regValue & 0xFF;
        regValue >>= 8;
    }

    /* Compute the CRC */
    if(useCRC != AD7124_DISABLE_CRC) {
        crc8 = AD7124_ComputeCRC8(wrBuf, reg._size + 1);
        wrBuf[reg._size + 1] = crc8;
    }

    /* Write data to the device */
    ret = AD7124_SPI_Write(wrBuf,
                    (useCRC != AD7124_DISABLE_CRC) ? reg._size + 2
                    : reg._size + 1);

    return ret;
}

/**
 * Reads one register, polling for SPI readiness when enabled.
 *
 * @param pReg Pointer to the register descriptor to update.
 * @return 0 on success or a negative error code.
 */
int32_t AD7124_ReadRegister(ad7124_st_reg* pReg)
{
    int32_t ret;

    if (pReg->addr != ERR_REG && check_ready) { // Skip polling for ERR_REG because the polling path itself reads the error register.
        ret = AD7124_WaitForSpiReady(spi_rdy_poll_cnt);
        if (ret < 0)
            return ret;
    }
    ret = AD7124_NoCheckReadRegister(pReg);

    return ret;
}

/**
 * Writes one register, polling for SPI readiness when enabled.
 *
 * @param pReg Register descriptor containing the value to write.
 * @return 0 on success or a negative error code.
 */
int32_t AD7124_WriteRegister(ad7124_st_reg pReg)
{
    int32_t ret;

    if (check_ready) {
        ret = AD7124_WaitForSpiReady(spi_rdy_poll_cnt);
        if (ret < 0)
            return ret;
    }
    ret = AD7124_NoCheckWriteRegister(pReg);

    return ret;
}

/**
 * Reads and returns the value of a shadowed device register.
 *
 * @param reg Register index to read.
 * @return Shadowed register value.
 */
uint32_t AD7124_ReadDeviceRegister(enum ad7124_registers reg)
{
    AD7124_ReadRegister(&regs[reg]);
    return (regs[reg].value);
}

/**
 * Updates a shadowed device register and writes it to hardware.
 *
 * @param reg Register index to write.
 * @param value Register value to store and transmit.
 * @return 0 on success or a negative error code.
 */
int32_t AD7124_WriteDeviceRegister(enum ad7124_registers reg, uint32_t value)
{
    regs[reg].value = value;
    return(AD7124_WriteRegister(regs[reg]));
}

/**
 * Issues the AD7124 serial-interface reset sequence.
 *
 * @return 0 on success or a negative error code.
 */
int32_t AD7124_Reset()
{
    int32_t ret = 0;
    uint8_t wrBuf[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    ret = AD7124_SPI_Write( wrBuf, 8); // 64 consecutive high bits reset the AD7124 serial interface.

    delay(200);

    return ret;
}

/**
 * Polls until the AD7124 can accept another SPI transaction.
 *
 * @param timeout Maximum poll count before returning.
 * @return 0 on success or a positive/negative timeout or error code.
 */
int32_t AD7124_WaitForSpiReady(uint32_t timeout)
{
    int32_t ret;
    int8_t _ready = 0;

    while(!_ready && --timeout) {
        /* Read the value of the Error Register */
        ret = AD7124_ReadRegister(&regs[AD7124_Error]);
        if(ret < 0)
            return ret;

        /* SPI readiness is inferred from the absence of the SPI IGNORE error latch. */
        _ready = (regs[AD7124_Error].value &
                 AD7124_ERR_REG_SPI_IGNORE_ERR) == 0;
    }

    return timeout ? 0 : 3;
}

/**
 * Polls until a new conversion result is available.
 *
 * @param timeout Maximum poll count before returning.
 * @return 0 on success or a negative timeout/error code.
 */
int32_t AD7124_WaitForConvReady(uint32_t timeout)
{
    int32_t ret;
    int8_t _ready = 0;

    while(!_ready && --timeout) {
      ret = AD7124_ReadRegister(&regs[AD7124_Status]); // teensy 3.2(96MHz) takes 6us
      if(ret < 0){
        return ret;
      }
          
      /* The AD7124 RDY bit is active-low, so 0 means a conversion is ready to read. */
      _ready = (regs[AD7124_Status].value &
                AD7124_STATUS_REG_RDY) == 0;
      delayMicroseconds(100);
    }
    return timeout ? 0 : -1;
}

/**
 * Reads the current conversion result register.
 *
 * @param pData Output pointer for the conversion result.
 * @return 0 on success or a negative error code.
 */
int32_t AD7124_ReadData( int32_t* pData)
{
    int32_t ret       = 0;
    uint8_t _buffer[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t i         = 0;
    ad7124_st_reg *pReg;

    if( !pData)
        return INVALID_VAL;

    pReg = &regs[AD7124_Data];

    /* Build the AD7124 communication byte: WEN | RD | data register address. */
    _buffer[0] = AD7124_COMM_REG_WEN | AD7124_COMM_REG_RD |
                AD7124_COMM_REG_RA(pReg->addr);

     SPI_Read(1, _buffer, pReg->_size + 1); // Preserve the existing direct target selection used by this read path.


    if(ret < 0)
        return ret;

    /* Fold the returned conversion bytes into the output value MSB-first. */
    *pData = 0;
    for(i = 1; i < pReg->_size + 1; i++) {
        *pData <<= 8;
        *pData += _buffer[i];
    }
    return ret;
}

/**
 * Computes the CRC checksum for a buffer.
 *
 * @param pBuf Data buffer.
 * @param bufSize Buffer length in bytes.
 * @return Computed CRC byte.
 */
uint8_t AD7124_ComputeCRC8(uint8_t * pBuf, uint8_t bufSize)
{
    uint8_t i   = 0;
    uint8_t crc = 0;

    while(bufSize) {
        for(i = 0x80; i != 0; i >>= 1) {
            if(((crc & 0x80) != 0) != ((*pBuf & i) != 0)) { /* MSB of CRC register XOR input Bit from Data */
                crc <<= 1;
                crc ^= AD7124_CRC8_POLYNOMIAL_REPRESENTATION;
            } else {
                crc <<= 1;
            }
        }
        pBuf++;
        bufSize--;
    }
    return crc;
}


/**
 * Derives the SPI-ready polling mode from the enabled error bits.
 *
 * @return None.
 */
void AD7124_UpdateDevSpiSettings()
{
    if (regs[AD7124_Error_En].value & AD7124_ERREN_REG_SPI_IGNORE_ERR_EN) { // Enabling this bit makes software poll before transfers.
       check_ready = 1;
    } else {
        check_ready = 0;
    }
}

/**
 * Resets the AD7124 and writes the configured startup register set.
 *
 * @return 0 on success or a negative error code.
 */
int32_t AD7124_Setup()
{
    int32_t ret;
    enum ad7124_registers regNr;

    spi_rdy_poll_cnt = 25000;

    /*  Reset the device interface.*/
    ret = AD7124_Reset();
    if (ret < 0)
        return ret;

    check_ready = 0;

    /* Initialize writable startup registers from status through the filter bank, before offset/gain calibration registers. */
    for(regNr = AD7124_Status; (regNr < AD7124_Offset_0) && !(ret < 0);regNr = static_cast<ad7124_registers>(regNr + 1)) {
        if (regs[regNr].rw == AD7124_RW) {
            ret = AD7124_WriteRegister(regs[regNr]);
            if (ret < 0)
                break;
        }

        /* Get CRC State and device SPI interface settings */
        if (regNr == AD7124_Error_En) {
            AD7124_UpdateDevSpiSettings();
        }
    }

    return ret;
}
/**
 * Dispatches a read transfer to the currently selected SPI target.
 *
 * @param data Transfer buffer.
 * @param bytes_number Number of bytes to transfer.
 * @return Number of bytes transferred.
 */
uint8_t AD7124_SPI_Read(uint8_t *data, uint8_t bytes_number)
{
   if(convFlag == 0)
      SPI_Read(0, data, bytes_number);
   else
      SPI_Read(1, data, bytes_number); // convFlag selects which SPI target path is active.

    return bytes_number;
}

/**
 * Dispatches a write transfer to the currently selected SPI target.
 *
 * @param data Transfer buffer.
 * @param bytes_number Number of bytes to transfer.
 * @return Number of bytes transferred.
 */
uint8_t AD7124_SPI_Write(uint8_t *data, uint8_t bytes_number)
{
   if(convFlag == 0)
      SPI_Write(0, data, bytes_number);
   else
      SPI_Write(1, data, bytes_number); // convFlag selects which SPI target path is active.

   return bytes_number;

}

/**
 * Reserved stub for future AD7124 sync/filter-reset support.
 *
 * @return None.
 */
void ad7124_sync(){
    // will do in the future when we need continuous conversion (higher SPS)
}
