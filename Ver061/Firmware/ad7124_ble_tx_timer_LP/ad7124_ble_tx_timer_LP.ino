// BLE-triggered AD7124 acquisition loop: rotates an external MAX4734 mux across
// four inputs, formats each full scan as ASCII, and batches scans for BLE UART transmission.

// These define's must be placed at the beginning before #include "NRF52TimerInterrupt.h"
// _TIMERINTERRUPT_LOGLEVEL_ from 0 to 4
// Don't define _TIMERINTERRUPT_LOGLEVEL_ > 0. Only for special ISR debugging only. Can hang the system.
// Don't define TIMER_INTERRUPT_DEBUG > 2. Only for special ISR debugging only. Can hang the system.
#define TIMER_INTERRUPT_DEBUG         0
#define _TIMERINTERRUPT_LOGLEVEL_     0

#include "Communication.h"
#include "AD7124.h"
#include "AD7124_regs.h"
#include <SPI.h>
#include "CN0391.h"
#include "max4734.h"
#include "Arduino.h"
#include <bluefruit.h>
#include "Adafruit_TinyUSB.h"

// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
#include "NRF52TimerInterrupt.h"
#include "NRF52_ISR_Timer.h"


# define CHANNELNUM 4
# define SAMPLECOMBINE 5

const uint8_t ADC_ch_fix = 0;                    // Fixed AD7124 logical channel used while the external mux selects the physical input.
const uint8_t ch_list[CHANNELNUM] = {0,1,2,3};  // External mux channel order for one full scan.
const uint8_t ch_idx_init = 0;                   // Scan start index used to detect frame boundaries.
volatile uint8_t ch_idx = ch_idx_init;           // Current mux position shared with the acquisition loop.
volatile uint8_t SAMPLECOMBINE_cnt = 0;          // Counts completed full scans before one BLE write.
char ble_buffer[244] = {0};                      // Aggregated ASCII BLE payload buffer.
char sample_buffer[CHANNELNUM*(8+1)] = {0};      // One full scan of fixed-width channel samples plus separators.
volatile int value;                              // Latest raw ADC code returned by the current conversion.
char *buff_pnt = sample_buffer;                  // Write cursor into the current scan buffer.
char *ble_buff_pnt = ble_buffer;                 // Write cursor into the aggregated BLE buffer.
volatile bool DataValid = true;                  // Drops the current scan if any channel read returns an invalid sentinel.
volatile bool ConfigComplete = false;            // Blocks loop activity until the connect callback finishes hardware setup.
volatile bool TimeUp = true;                     // Sampling tick flag raised asynchronously by the timer path.

volatile bool SerialSwitch = false;   // Only for printing log output, not for telemetry transmission.

BLEUart bleuart;                                 // BLE UART service used for telemetry transmission.

NRF52Timer ITimer0(NRF_TIMER_4);                 // Hardware timer used to service the ISR timer scheduler. NOTE: We do not use any timer in this implementation. Ignore all locations where it appears.

// Init NRF52_ISR_Timer
// Each NRF52_ISR_Timer can service 16 different ISR-based timers
NRF52_ISR_Timer ISR_Timer;                       // ISR-safe software timer dispatcher.

SPISettings MyFastSPI = SPISettings(F_CPU / SPI_CLOCK_DIV2, MSBFIRST, SPI_MODE3);  // SPI settings required by the AD7124 path.


/**
 * Services the ISR timer dispatcher from the hardware timer callback.
 *
 * @return None.
 */
void TimerHandler0(){
  ISR_Timer.run();
}

/**
 * Raises the sampling-ready flag consumed by the main acquisition loop.
 *
 * @return None.
 */
void TimeInterrupt_service()
{
  TimeUp = true;
}


/**
 * Initializes GPIO, SPI, BLE UART, and advertising for the streaming firmware path.
 *
 * @return None.
 */
void setup() {
  // These delays preserve the established CS/SYNC bring-up timing for the front-end hardware.
  pinMode(CS_PIN, OUTPUT);  // pinmode change must have a delay at least 100us. 10us is not enough!
  delayMicroseconds(100);
  digitalWrite(CS_PIN, HIGH);
  delayMicroseconds(100);
  pinMode(SYNC_PIN, OUTPUT);
  digitalWrite(SYNC_PIN, HIGH);
  delayMicroseconds(1000);  // CS_pin selection must have a delay at least 1ms. 100us is not enough!

  memset(ble_buffer, 0, 244);
  memset(sample_buffer, 0, CHANNELNUM*(8+1));

  if (SerialSwitch){
    Serial.begin(115200);
    while ( !Serial ) delay(10);   // for nrf52840 with native usb

    Serial.println("Peripheral BLEUART Tx");
    Serial.println("-----------------------------------\n");
  }
  
  // Prime the SPI peripheral once during boot so later transactions begin from a known state.
  SPI.begin();
  SPI.end();

  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behaviour, but provided
  // here in case you want to control this manually via PIN 19
  Bluefruit.autoConnLed(false);

  // Config the peripheral connection with maximum bandwidth
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(0);    // Check bluefruit.h for supported values
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  Bluefruit.Periph.setConnInterval(6, 12); // 7.5 - 15 ms

  // Configure and Start BLE Uart Service
  bleuart.begin();

  // Set up and start advertising
  startAdv();
}

/**
 * Configures the advertising and scan-response payload for the BLE UART service.
 *
 * @return None.
 */
void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // There is no room for Name in Advertising packet
  // Use Scan response for Name
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

/**
 * Negotiates the BLE link and brings up the mux/ADC path for streaming.
 *
 * @param conn_handle Active BLE connection handle supplied by Bluefruit.
 * @return None.
 */
void connect_callback(uint16_t conn_handle)
{
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  

  if (SerialSwitch){
    Serial.println("Connected");

    Serial.println(conn->getPHY());
    Serial.println(conn->getDataLength());
    Serial.println(conn->getMtu());
  }

  // Keep the BLE link-upgrade requests grouped before acquisition begins.
  conn->requestPHY();
  conn->requestDataLengthUpdate();
  conn->requestMtuExchange(247);

  // Let PHY / data-length / MTU negotiation settle before ADC traffic starts.
  delay(1000);

  
  if (SerialSwitch){
    Serial.println(conn->getPHY());
    Serial.println(conn->getDataLength());
    Serial.println(conn->getMtu());
  }
  

  // Start the SPI transaction before mux and ADC configuration so chip-select timing stays unchanged.
  SPI.beginTransaction(MyFastSPI);

  delayMicroseconds(100);

  max4734_init();
  max4734_SelectChannel(ch_list[ch_idx]);

	digitalWrite(CS_PIN, LOW);

  CN0391_init();
  CN0391_enable_channel(ADC_ch_fix);

  digitalWrite(CS_PIN, HIGH);
  SPI.end();

  // Interval in microsecs
  if (ITimer0.attachInterruptInterval(1000, TimerHandler0))
  {
    if (SerialSwitch){
      Serial.print(F("Starting  ITimer0 OK, millis() = "));
      Serial.println(millis());
    }
  }
  else{
    if (SerialSwitch){
      Serial.println(F("Can't set ITimer0. Select another freq. or timer"));
    }
  }
  ITimer0.stopTimer();

  ConfigComplete = true;  // This Connect Callback function is not blocked and of low interruption priority,
                          // and the program will enter the loop function before it's completed.
                          // This flag will make sure the loop function runs after all configurations are done.
}

/**
 * Handles disconnect events and blocks acquisition until the next setup pass.
 *
 * @param conn_handle Connection where this event happens.
 * @param reason BLE HCI status code describing the disconnect.
 * @return None.
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  if (SerialSwitch){
    Serial.println();
    Serial.print("Disconnected, reason = 0x");
    Serial.println(reason, HEX);
  }
  

  ConfigComplete=false;
}

/**
 * Performs one ADC acquisition step, assembles full scan frames, and flushes batches over BLE.
 *
 * @return None.
 */
void loop(){

  if (ConfigComplete){
    if (TimeUp){
      if (Bluefruit.connected()){
        SPI.beginTransaction(MyFastSPI);

        digitalWrite(CS_PIN, LOW);
        value = read_channel(ADC_ch_fix);
        if ((value == 0)||(value == 1)){
          DataValid = false;
        }

        uint8_t ch_idx_new = (ch_idx + 1) % CHANNELNUM;
        max4734_SelectChannel(ch_list[ch_idx_new]);
        
        // Preserve the mux settle delay before the next conversion is restarted on the new input.
        delayMicroseconds(10);
        start_channel_conv(ADC_ch_fix);

        digitalWrite(CS_PIN, HIGH);
        SPI.end();

        sprintf(buff_pnt, "%08d", value);
        buff_pnt += 8;
        sprintf(buff_pnt," ");
        buff_pnt += 1;

        if (ch_idx_new==ch_idx_init){  // Reaching the initial index marks the end of one full channel scan.
          if (DataValid){
            // Capture one timestamp per completed scan frame, not per channel sample.
            sprintf(ble_buff_pnt,"%010d",micros());
            ble_buff_pnt += 10;
            sprintf(ble_buff_pnt," ");
            ble_buff_pnt += 1;
            sprintf(ble_buff_pnt,sample_buffer);
            ble_buff_pnt += sizeof(sample_buffer);
            sprintf(ble_buff_pnt,"\n");
            ble_buff_pnt += 1;

            SAMPLECOMBINE_cnt = (SAMPLECOMBINE_cnt + 1) % SAMPLECOMBINE;
          }
          else{
            DataValid = true;
          }

          memset(sample_buffer, 0, CHANNELNUM*(8+1));
          buff_pnt = sample_buffer;
        }

        ch_idx = ch_idx_new;

        
        if (SAMPLECOMBINE_cnt==0){  // Flush only after SAMPLECOMBINE complete scan frames have been accumulated.
          bleuart.write(ble_buffer);
          memset(ble_buffer, 0, 244);
          ble_buff_pnt = ble_buffer;
        }
      }
    }
  }

  waitForEvent();  // Idle until the next BLE or timer-driven event wakes the CPU.

  delay(5);   // Yield between iterations without changing the acquisition sequence.
}
