// CN0391/AD7124 acquisition declarations for the muxed sensor front end.
// This interface exposes raw ADC helpers plus RTD / thermocouple conversion utilities.

#ifndef _CN0391_H_
#define _CN0391_H_

#include "AD7124.h"
#include "PROGMEM_readAnything.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum
{
	CHANNEL_P1 = 0,
	CHANNEL_P2,
	CHANNEL_P3,
	CHANNEL_P4
}channel_t;

typedef enum
{
  ERR_UNDER_RANGE = 1,
  ERR_OVER_RANGE,
  NO_ERR

}error_code;

/**
 * Reads one logical CN0391 channel and returns the raw bipolar ADC code.
 *
 * @param ch Logical channel identifier used by the higher-level CN0391 API.
 * @return Raw ADC code for the requested channel.
 */
int32_t CN0391_read_channel(int ch);

/**
 * Converts a raw ADC code into millivolts using the reference/gain for the selected path.
 *
 * @param data Raw AD7124 conversion code.
 * @param channel RTD or thermocouple path selector.
 * @return Converted channel voltage in millivolts.
 */
float CN0391_data_to_voltage(int32_t data, uint8_t channel);

/**
 * Converts a raw RTD-path ADC code into resistance using the front-end assumptions.
 *
 * @param data Raw AD7124 conversion code.
 * @return Computed RTD resistance in ohms.
 */
float CN0391_data_to_resistance(int32_t data);

/**
 * Computes RTD temperature for the selected logical channel.
 *
 * @param ch Logical RTD channel.
 * @param temp Output pointer for the computed temperature.
 * @return None.
 */
void CN0391_calc_rtd_temperature(channel_t ch, float *temp);

/**
 * Computes thermocouple temperature using cold-junction compensation.
 *
 * @param ch Logical thermocouple channel.
 * @param cjTemp Cold-junction temperature input.
 * @param buffer Output pointer for the computed thermocouple temperature.
 * @return None.
 */
void CN0391_calc_th_temperature(channel_t ch, float cjTemp, float *buffer);

/**
 * Enables one AD7124 channel map entry.
 *
 * @param channel AD7124 channel index.
 * @return None.
 */
void CN0391_enable_channel(int channel);

/**
 * Disables one AD7124 channel map entry.
 *
 * @param channel AD7124 channel index.
 * @return None.
 */
void CN0391_disable_channel(int channel);

/**
 * Routes the selected AD7124 excitation current output.
 *
 * @param current_source_channel Current source selector encoded for the AD7124 register map.
 * @return None.
 */
void CN0391_enable_current_source(int current_source_channel);

/**
 * Retriggers a single AD7124 conversion by rewriting the active control register state.
 *
 * @return None.
 */
void CN0391_start_single_conversion();

/**
 * Resets the AD7124 device.
 *
 * @return None.
 */
void CN0391_reset();

/**
 * Applies the configured AD7124 setup sequence.
 *
 * @return None.
 */
void CN0391_setup();

/**
 * Performs the CN0391 initialization sequence used by this firmware path.
 *
 * @return None.
 */
void CN0391_init();

/**
 * Runs the legacy calibration helper for one channel.
 *
 * @param channel Channel index to calibrate.
 * @return None.
 */
void CN0391_calibration(uint8_t channel);

/**
 * Reads back the shadowed AD7124 register set for inspection.
 *
 * @return None.
 */
void CN0391_read_reg(void);

/**
 * Writes a calibration mode value into the AD7124 control register.
 *
 * @param mode AD7124 ADC control mode value.
 * @return None.
 */
void CN0391_set_calibration_mode(uint16_t mode);

/**
 * Updates the AD7124 power mode bits.
 *
 * @param mode Encoded AD7124 power mode value.
 * @return None.
 */
void CN0391_set_power_mode(int mode);

/**
 * Legacy helper that starts and reads channel 0 once.
 *
 * @return Raw ADC code from the helper path.
 */
int CN0391_set_data(void);

/**
 * Legacy display helper retained for interface compatibility.
 *
 * @return None.
 */
void CN0391_display_data(void);

/**
 * Starts the next conversion for the current acquisition path.
 *
 * @param ch Logical acquisition channel placeholder kept for interface compatibility.
 * @return None.
 */
void start_channel_conv(int ch);

/**
 * Waits for conversion ready and reads the raw ADC code.
 *
 * @param ch Logical acquisition channel placeholder kept for interface compatibility.
 * @return Raw AD7124 conversion code or a timeout sentinel.
 */
int32_t read_channel(int ch);

/**
 * Converts a raw bipolar AD7124 code into volts for a provided reference/gain pair.
 *
 * @param value Raw AD7124 code.
 * @param mVref Reference voltage in millivolts.
 * @param pga_gain Applied PGA gain.
 * @return Converted voltage.
 */
double toVolt(double value,double mVref,double pga_gain);

extern volatile bool SerialSwitch;  // Gates timeout/debug serial prints from the acquisition path.


#define YES    1
#define NO     0

#define R5      1600.0  // Front-end reference resistor in ohms.
#define I_EXT   0.75    // External excitation current in mA.

#define VREF_EXT    (R5*I_EXT)  // Derived external reference voltage in mV.
#define VREF_INT     2500.0     // Internal AD7124 reference voltage in mV.

#define _2_23     8388608.0     // Midscale of the bipolar 24-bit coding used by the AD7124.

#define RTD_CHANNEL    0

#define TH_CHANNEL     1

#define GAIN_RTD       1   // PGA gain used by the RTD path.
#define GAIN_TH        32  // PGA gain used by the thermocouple path.


#define POLY_CALC(retVal, inVal, coeff_array) \
{ \
    float expVal = 1.0f; \
    const float* coeff = coeff_array; \
    retVal = 0.0f; \
    while(*coeff != 1.0f)\
    { \
        retVal += *coeff * expVal; \
        expVal *= inVal; \
        coeff++; \
    }\
} // Coefficient arrays terminate with a sentinel value of 1.0f.

#define USE_RTD_CALIBRATION  YES  // Set YES to enable calibration on the RTD channel.
#define USE_TH_CALIBRATION   YES  // Set YES to enable calibration on the thermocouple channel.

#define DISPLAY_REFRESH     (1000)   // Display refresh period in milliseconds for the legacy demo path.

#define TC_OFFSET_VOLTAGE    0.00    // Thermocouple offset compensation in millivolts.

#ifdef  __cplusplus
}
#endif // __cplusplus

#endif
