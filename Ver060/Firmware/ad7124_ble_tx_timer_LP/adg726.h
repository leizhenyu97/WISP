// Legacy ADG726 mux declarations retained for alternate hardware wiring.

#ifndef _ADG726_H
#define _ADG726_H

#include "Arduino.h"

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * Selects one ADG726 channel by driving the four address pins directly.
 *
 * @param CurrentChannel Logical mux channel index.
 * @return None.
 */
void SelectChannel(unsigned char CurrentChannel);

/**
 * Configures the ADG726 address GPIO pins.
 *
 * @return None.
 */
void adg726_init();


#ifdef  __cplusplus
}
#endif // __cplusplus

#endif
