// MAX4734 control helpers for the active external analog channel switch.

#include "max4734.h"
#include "Arduino.h"
#include <stdint.h>
#include "Communication.h"

/**
 * Selects one mux input using the board-specific logical-to-physical channel remap.
 *
 * @param CurrentChannel Logical channel index in the firmware scan order.
 * @return None.
 */
void max4734_SelectChannel(unsigned char CurrentChannel){
    const uint8_t ch_mapping[4] = {3,2,0,1};  // Maps firmware channel indices to the actual MAX4734 select truth table on this board.

    digitalWrite(0,(ch_mapping[CurrentChannel] & 0b00000001)); // GPIO 0 drives the low select bit; zero/nonzero becomes LOW/HIGH.
    digitalWrite(1,(ch_mapping[CurrentChannel] & 0b00000010)); // GPIO 1 drives the high select bit; zero/nonzero becomes LOW/HIGH.
    delayMicroseconds(20);  // Allow the analog switch address lines to settle before the next ADC action.
}

/**
 * Configures the GPIO lines that drive the MAX4734 select pins.
 *
 * @return None.
 */
void max4734_init(){
    pinMode(1,OUTPUT);
    pinMode(0,OUTPUT);
}
