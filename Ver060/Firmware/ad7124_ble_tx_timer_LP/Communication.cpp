// Minimal SPI transport helpers for the AD7124 path.
// Chip select ownership stays with the caller; these routines only move bytes.

#include "Communication.h"

uint8_t convFlag = 0; // Exported device selector placeholder; the current firmware keeps the single AD7124 path on index 0.

/**
 * Shifts a transmit buffer over SPI without toggling chip select locally.
 *
 * @param slaveDeviceId Placeholder selector kept for interface compatibility.
 * @param data Buffer containing bytes to transmit.
 * @param bytesNumber Number of bytes to shift out.
 * @return None.
 */
void SPI_Write(unsigned char slaveDeviceId, unsigned char* data, unsigned char bytesNumber)
{
  (void)slaveDeviceId;
  unsigned char count;

    for(count = 0;count < bytesNumber;count++)
    {
            SPI.transfer(data[count]);  // Clock one transmit byte while the caller-owned chip select remains active.
    }
}

/**
 * Clocks a command byte followed by dummy bytes to receive data over SPI.
 *
 * @param slaveDeviceId Placeholder selector kept for interface compatibility.
 * @param data Buffer holding the command byte on entry and received bytes on return.
 * @param bytesNumber Number of bytes to transfer.
 * @return None.
 */
void SPI_Read(unsigned char slaveDeviceId, unsigned char* data, unsigned char bytesNumber)
{
   (void)slaveDeviceId;
   unsigned char writeData[4]  = {0, 0, 0, 0};
   unsigned char count          = 0;

    for(count = 0;count < bytesNumber;count++)
    {
        if(count == 0)
           writeData[count] = data[count]; // Preserve the caller-provided command/register byte for the first transfer.
        else
           writeData[count] = 0xAA;    // Dummy fill byte used only to generate clocks for later read bytes.
    }

    for(count = 0;count < bytesNumber;count++)
    {
      data[count] =  SPI.transfer(writeData[count]); // SPI is full-duplex, so receive data arrives while dummy/command bytes are sent.
    }
}
