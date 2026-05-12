// Minimal MAX4734 channel-select declarations for the active low-power firmware path.

#ifndef _MAX4734_H
#define _MAX4734_H

#include "Arduino.h"

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * Selects one of the four MAX4734 inputs through the board-specific GPIO mapping.
 *
 * @param CurrentChannel Logical channel index expected by the firmware.
 * @return None.
 */
void max4734_SelectChannel(unsigned char CurrentChannel);

/**
 * Configures the two MAX4734 select GPIO pins as outputs.
 *
 * @return None.
 */
void max4734_init();


#ifdef  __cplusplus
}
#endif // __cplusplus

#endif
