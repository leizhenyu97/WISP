// Legacy ADG726 mux helpers kept for alternate board variants.

#include "adg726.h"
#include "Arduino.h"
#include <stdint.h>
#include "Communication.h"

/**
 * Selects one ADG726 channel by driving the four address lines.
 *
 * @param CurrentChannel Logical mux channel index.
 * @return None.
 */
void SelectChannel(unsigned char CurrentChannel){
    // Bits 0..3 of CurrentChannel directly drive GPIO 0..3 for the ADG726 address bus.
    digitalWrite(0,(CurrentChannel & 0b00000001));
    digitalWrite(1,(CurrentChannel & 0b00000010));
    digitalWrite(2,(CurrentChannel & 0b00000100));
    digitalWrite(3,(CurrentChannel & 0b00001000));
    delayMicroseconds(20);  // Preserve the post-address settling delay unless hardware timing is revalidated.
}

/**
 * Configures the ADG726 address GPIO pins.
 *
 * @return None.
 */
void adg726_init(){
    pinMode(3,OUTPUT);
    pinMode(2,OUTPUT);
    pinMode(1,OUTPUT);
    pinMode(0,OUTPUT);
}
