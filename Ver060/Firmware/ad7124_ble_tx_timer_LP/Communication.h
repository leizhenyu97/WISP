// Minimal SPI transport declarations for the AD7124 firmware path.
// Chip select timing is handled by the caller; these helpers only clock bytes.

#ifndef _COMMUNICATION_H
#define _COMMUNICATION_H

#include <SPI.h>
#include "Arduino.h"

#ifdef  __cplusplus
extern "C" {
#endif
/********************************* Internal defines ****************************/
#define CS_PIN       7
#define SYNC_PIN     4

extern uint8_t convFlag;

/**
 * Shifts a transmit buffer over SPI.
 *
 * @param slaveDeviceId Placeholder selector kept for interface compatibility.
 * @param data Buffer containing bytes to transmit.
 * @param bytesNumber Number of bytes to shift out.
 * @return None.
 */
void SPI_Write(unsigned char slaveDeviceId, unsigned char* data, unsigned char bytesNumber);

/**
 * Clocks a command byte plus dummy bytes to read data back over SPI.
 *
 * @param slaveDeviceId Placeholder selector kept for interface compatibility.
 * @param data Buffer holding the command byte on entry and received bytes on return.
 * @param bytesNumber Number of bytes to transfer.
 * @return None.
 */
void SPI_Read(unsigned char slaveDeviceId, unsigned char* data, unsigned char bytesNumber);

#ifdef  __cplusplus
}
#endif // __cplusplus

#endif
