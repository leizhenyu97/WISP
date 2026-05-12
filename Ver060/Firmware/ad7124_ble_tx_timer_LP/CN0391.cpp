// CN0391/AD7124 helpers for the muxed sensor front end.
// This file implements low-power single-conversion reads and the register updates used by the acquisition loop.

#include "Arduino.h"
#include "CN0391.h"
#include "AD7124.h"
#include "Communication.h"
#include "ad7124_regs.h"

int32_t _ADCValue0[4], _ADCValue1[4];                    // Per-slot raw ADC storage kept for legacy CN0391 processing paths.
float rRtdValue[4], temp0[4], temp1[4];                  // Per-slot RTD resistance and temperature working buffers.
float cj_Voltage[4], th_Voltage_read[4], th_Voltage[4];  // Per-slot cold-junction and thermocouple voltage buffers.
static const unsigned char thermocouple_type[] = { 'T', 'J', 'K', 'E', 'S', 'R', 'N', 'B' }; // Type index to letter-code map.
uint8_t th_types[4];                                     // Thermocouple type selection for each logical slot.
error_code errFlag[4] = {NO_ERR, NO_ERR, NO_ERR, NO_ERR}; // Per-slot conversion status for the legacy temperature helpers.

#define ms_delay (1)  // Post-register-write settling delay for AD7124 state changes.

/**
 * Starts the next single conversion for the active ADC path.
 *
 * @param ch Logical channel placeholder kept for interface compatibility.
 * @return None.
 */
void start_channel_conv(int ch)
{
  (void)ch;
  CN0391_start_single_conversion();
}

/**
 * Waits for conversion ready and returns the raw ADC code.
 *
 * @param ch Logical channel placeholder kept for interface compatibility.
 * @return Raw AD7124 code, or the timeout sentinel used by the caller.
 */
int32_t read_channel(int ch)
{
  int32_t data;

  (void)ch;
  if (AD7124_WaitForConvReady(100) == -1) {  // Use the existing poll budget and return sentinel 1 on timeout for the loop logic.
    if (SerialSwitch){
      Serial.println("TIMEOUT");
    }
    return 1;
  }

  AD7124_ReadData(&data); // teensy 3.2(96MHz) takes 10us
  return data;
}

/**
 * Converts a raw ADC code into millivolts for the selected measurement path.
 *
 * @param data Raw AD7124 conversion code.
 * @param channel RTD or thermocouple path selector.
 * @return Converted channel voltage in millivolts.
 */
float CN0391_data_to_voltage(int32_t data, uint8_t channel) {

  float voltage;

  if(channel == RTD_CHANNEL){
        voltage = (VREF_EXT*(data - _2_23))/(_2_23 *GAIN_RTD); // Bipolar 24-bit coding is centered at 2^23 for the RTD path.
  } else{
        voltage = (VREF_INT*(data - _2_23))/(_2_23*GAIN_TH);   // Bipolar 24-bit coding is centered at 2^23 for the thermocouple path.
  }

  return voltage;
}

/**
 * Converts a raw RTD-path ADC code into resistance.
 *
 * @param data Raw AD7124 conversion code.
 * @return Computed RTD resistance in ohms.
 */
float CN0391_data_to_resistance(int32_t data)
{
  float rRtd;

  rRtd = (R5*(data - _2_23))/(_2_23 *GAIN_RTD);

  return rRtd;
}

/**
 * Enables one AD7124 channel map entry.
 *
 * @param channel AD7124 channel index.
 * @return None.
 */
void CN0391_enable_channel(int channel)
{
  enum ad7124_registers regNr = static_cast<enum ad7124_registers> (AD7124_Channel_0 + channel); // Select the channel map register to update.
  uint32_t setValue = AD7124_ReadDeviceRegister(regNr);
  setValue |= (uint32_t)AD7124_CH_MAP_REG_CH_ENABLE;  // Enable the selected AD7124 channel map entry.
  setValue &= 0xFFFF;                                 // Truncate to the 16-bit channel register width before writeback.
  AD7124_WriteDeviceRegister(regNr, setValue);        // Write data to the AD7124 shadow register and device.
  delay(ms_delay);
}

/**
 * Disables one AD7124 channel map entry.
 *
 * @param channel AD7124 channel index.
 * @return None.
 */
void CN0391_disable_channel(int channel)
{
  enum ad7124_registers regNr = static_cast<enum ad7124_registers> (AD7124_Channel_0 + channel); // Select the channel map register to update.
  uint32_t setValue = AD7124_ReadDeviceRegister(regNr);
  setValue &= (~(uint32_t) AD7124_CH_MAP_REG_CH_ENABLE); // Clear the enable bit for the selected channel map entry.
  setValue &= 0xFFFF;                                    // Truncate to the 16-bit channel register width before writeback.
  AD7124_WriteDeviceRegister(regNr, setValue);           // Write data to the AD7124 shadow register and device.
  delay(ms_delay);
}

/**
 * Routes the selected AD7124 excitation current output.
 *
 * @param current_source_channel Current source selector encoded for the AD7124 register map.
 * @return None.
 */
void CN0391_enable_current_source(int current_source_channel)
{
  enum ad7124_registers regNr = AD7124_IOCon1; // Select the AD7124 IO control register that owns the excitation current routing.
  uint32_t setValue = AD7124_ReadDeviceRegister(regNr);
  setValue &= ~(AD7124_IO_CTRL1_REG_IOUT_CH0(0xF));
  setValue |= AD7124_IO_CTRL1_REG_IOUT_CH0(2*current_source_channel + 1); // AD7124 encoding maps the helper index to the IOUT_CH0 selection field.
  setValue &= 0xFFFFFF;                                                   // Truncate to the 24-bit IO control register width before writeback.
  AD7124_WriteDeviceRegister(regNr, setValue);                            // Write data to the AD7124 shadow register and device.
  delay(ms_delay);
}

/**
 * Retriggers a single AD7124 conversion by rewriting the active control register.
 *
 * @return None.
 */
void CN0391_start_single_conversion()
// NOTE: rewrite the adc_control or config reg will reset the digital filter and the
// modulator of AD7124
{
  enum ad7124_registers regNr = AD7124_ADC_Control; // Select the ADC control register that retriggers the single-conversion path.
  AD7124_WriteRegister(regs[regNr]);
}

/**
 * Resets the AD7124 device and prints the result when serial logging is enabled.
 *
 * @return None.
 */
void CN0391_reset() {

  AD7124_Reset();
  Serial.println(F("Reseted AD7124\n"));
}

/**
 * Applies the configured AD7124 setup sequence.
 *
 * @return None.
 */
void CN0391_setup() {

  AD7124_Setup();
}

/**
 * Runs the CN0391 initialization sequence used by this firmware path.
 *
 * @return None.
 */
void CN0391_init() {
  CN0391_setup();

  delay(ms_delay);
}

/**
 * Converts a raw bipolar AD7124 code into volts for the provided reference and gain.
 *
 * @param value Raw AD7124 conversion code.
 * @param mVref Reference voltage in millivolts.
 * @param pga_gain Applied PGA gain.
 * @return Converted voltage.
 */
double toVolt(double value,double mVref,double pga_gain)
{
  double voltage = (double)value;
    
  voltage = (value/(1<<23)-1)*mVref/pga_gain;

  return voltage;
}

/**
 * Legacy helper that triggers and reads channel 0 once.
 *
 * @return Raw ADC code from the helper path.
 */
int CN0391_set_data(void)
{
  int i=0;
   
  start_channel_conv(i);

  return read_channel(i);
}

/**
 * Reads back the AD7124 register set for inspection.
 *
 * @return None.
 */
void CN0391_read_reg(void)
{
  enum ad7124_registers regNr;

  for(regNr = AD7124_Status; regNr < AD7124_REG_NO;regNr = static_cast<enum ad7124_registers>(regNr + 1)) {

    AD7124_ReadDeviceRegister(regNr);

  }
}

/**
 * Writes a calibration mode value into the AD7124 control register and waits for completion.
 *
 * @param mode AD7124 ADC control mode value.
 * @return None.
 */
void CN0391_set_calibration_mode(uint16_t mode)
{
  AD7124_WriteDeviceRegister(AD7124_ADC_Control, mode);

  if (AD7124_WaitForConvReady(100000) == -3) {
    Serial.println("TIMEOUT");
  }
}

/**
 * Updates the AD7124 power mode bits.
 *
 * @param mode Encoded AD7124 power mode value.
 * @return None.
 */
void CN0391_set_power_mode(int mode)
{
  enum ad7124_registers regNr = AD7124_ADC_Control;      // Select the ADC control register that owns the power mode bits.
  uint32_t setValue = AD7124_ReadDeviceRegister(regNr);
  setValue |= AD7124_ADC_CTRL_REG_POWER_MODE(mode);      // Set the requested AD7124 low-power mode bits.
  setValue &= 0xFFFF;                                    // Truncate to the 16-bit control register width before writeback.
  AD7124_WriteDeviceRegister(regNr, setValue);           // Write data to the AD7124 shadow register and device.
  delay(ms_delay);
}
