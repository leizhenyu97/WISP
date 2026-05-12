#include <bluefruit.h>
#include "Adafruit_TinyUSB.h"


BLEClientUart clientUart; // bleuart client

char ble_buffer[244] = {0};


void setup()
{
  memset(ble_buffer, 0, 244);

  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  // Serial.println("Central BLEUART Rx");
  // Serial.println("-----------------------------------\n");


  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behaviour, but provided
  // here in case you want to control this manually via PIN 19
  Bluefruit.autoConnLed(true);

  // Config the connection with maximum bandwidth 
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configCentralBandwidth(BANDWIDTH_MAX);  
  
  // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 1
  // SRAM usage required by SoftDevice will increase dramatically with number of connections
  Bluefruit.begin(0, 1);
  
  Bluefruit.setName("Bluefruit52 Central");

  // Init BLE Central Uart Serivce
  clientUart.begin();
  clientUart.setRxCallback(bleuart_rx_callback);  

  // Increase Blink rate to different from PrPh advertising mode
  Bluefruit.setConnLedInterval(250);

  // Callbacks for Central
  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);

  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Interval = 100 ms, window = 80 ms
   * - Don't use active scan
   * - Start(timeout) with timeout = 0 will scan forever (until connected)
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);                   // // 0 = Don't stop scanning after n seconds
}

/**
 * Callback invoked when scanner pick up an advertising data
 * @param report Structural advertising data
 */
void scan_callback(ble_gap_evt_adv_report_t* report)
{
  // Check if advertising contain BleUart service
  if ( Bluefruit.Scanner.checkReportForService(report, clientUart) )
  {
    // Serial.print("BLE UART service detected. Connecting ... ");

    // Connect to device with bleuart service in advertising
    Bluefruit.Central.connect(report);
  }else
  {      
    // For Softdevice v6: after received a report, scanner will be paused
    // We need to call Scanner resume() to continue scanning
    Bluefruit.Scanner.resume();
  }
}

/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
void connect_callback(uint16_t conn_handle)
{
  // Serial.println("Connected");

  // Serial.print("Discovering BLE Uart Service ... ");
  if ( clientUart.discover(conn_handle) )
  {
    // Serial.println("Found it");

    // Serial.println("Enable TXD's notify");
    clientUart.enableTXD();

    // Serial.println("Ready to receive from peripheral");
  }else
  {
    // Serial.println("Found NONE");
    
    // disconnect since we couldn't find bleuart service
    Bluefruit.disconnect(conn_handle);
  }  

  if (clientUart.available()){
    clientUart.flush();
  }
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
  
  // Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}

/**
 * Callback invoked when uart received data
 * @param uart_svc Reference object to the service where the data 
 * arrived. In this example it is clientUart
 */ 
void bleuart_rx_callback(BLEClientUart& uart_svc)
{

  if(uart_svc.available()){
    uart_svc.read(ble_buffer,244);
    uart_svc.flush(); // empty rx fifo
    Serial.print(ble_buffer);
    memset(ble_buffer, 0, 244);
  }

  

}


void loop()
{
  delay(5000);
}
