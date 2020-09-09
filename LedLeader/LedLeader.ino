/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/*
 * This sketch demonstrate how to run both Central and Peripheral roles
 * at the same time. It will act as a relay between an central (mobile)
 * to another peripheral using bleuart service.
 * 
 * Mobile <--> DualRole <--> peripheral Ble Uart
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <bluefruit.h>

#define NEOPIXEL_VERSION_STRING "Neopixel v2.0"

/* Pin used to drive the NeoPixels */
#define PIN 5

// pixel configurations
#define MAXCOMPONENTS  4
uint8_t *pixelBuffer = NULL;
uint8_t width = 0;
uint8_t height = 0;
uint8_t stride;
uint8_t componentsValue;
bool is400Hz;
uint8_t components = 3;     // only 3 and 4 are valid values

int NUM_OF_PIXELS = 1;
Adafruit_NeoPixel neopixel = Adafruit_NeoPixel(NUM_OF_PIXELS, PIN, NEO_GRB);

// BLE Service
BLEDfu  bledfu;
BLEDis  bledis;
BLEUart bleuart;

// Central uart client
BLEClientUart clientUart;

// sets the attributes of the leader and followers

// uncomment to connect with Alpha Follower
// const char* NAME = "AlphaLeader";
// const char* FOLLOWER_NAME = "AlphaFollower";
// uint16_t FOLLOWER_UUID = 0x3826;

// uncomment to connect with Beta Follower
const char* NAME = "BetaLeader";
const char* FOLLOWER_NAME = "BetaFollower";
uint16_t FOLLOWER_UUID = 0x1844;

// track connection handle to see if they change?
uint16_t mobileCHandle = -1;
uint16_t followerCHandle = -1;

void setup()
{
  Serial.begin(115200);

  Serial.println("Bluefruit52 Dual Role MVP");
  Serial.println("-------------------------------------\n");

  // configure neopixels
  neopixel.begin();
  
  // Initialize Bluefruit with max concurrent connections as Peripheral = 1, Central = 1
  // SRAM usage required by SoftDevice will increase with number of connections
  Bluefruit.begin(1, 1);
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName(NAME);

  // Callbacks for Peripheral
  Bluefruit.Periph.setConnectCallback(prph_connect_callback);
  Bluefruit.Periph.setDisconnectCallback(prph_disconnect_callback);

  // Callbacks for Central
  Bluefruit.Central.setConnectCallback(cent_connect_callback);
  Bluefruit.Central.setDisconnectCallback(cent_disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();
  // bleuart.setRxCallback(prph_bleuart_rx_callback);

  // Init BLE Central Uart Serivce
  clientUart.begin();
  // clientUart.setRxCallback(cent_bleuart_rx_callback);


  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Interval = 100 ms, window = 80 ms
   * - Filter only accept bleuart service
   * - Don't use active scan
   * - Start(timeout) with timeout = 0 will scan forever (until connected)
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
  Bluefruit.Scanner.filterUuid(BLEUuid(FOLLOWER_UUID));
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);                   // 0 = Don't stop scanning after n seconds

  // Set up and start advertising
  startAdv();

  // device should start off disconnected to both devices
  setRed();
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
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

void loop()
{
  if(!bleuart.notifyEnabled())
    return;

  // read in buffer from mobile device
  uint8_t str[20+1] = { 0 };
  bleuart.read(str, 10);  

  // integer for command
  int command = str[0];

  Serial.print("Received Command from mobile device: ");
  Serial.println(str[0]);

  // check command
  switch (command) {
    case 'V': {   // Get Version
      commandVersion();
      break;
    }

    case 'S': {   // Setup dimensions, components, stride...
      commandSetup(str);
      break;
    }

    case 'C': {   // Clear with color
      commandClearColor(str);
      break;
    }

    case 'B': {   // Set Brightness
      commandSetBrightness(str);
      break;
    }
          
    case 'P': {   // Set Pixel
      commandSetPixel(str);
      break;
    }

    // case 'I': {   // Receive new image
    //   commandImage();
    //   break;
    // }
  }

  if (clientUart.discovered())
  {
    clientUart.write(str, 10);
  }

  delay(10);  
}

/*------------------------------------------------------------------*/
/* Peripheral
 *------------------------------------------------------------------*/

void prph_connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char peer_name[32] = { 0 };
  connection->getPeerName(peer_name, sizeof(peer_name));

  Serial.print("[Prph] Connected to ");
  Serial.printf("Handle: %i\n", conn_handle);
  Serial.println(peer_name);

  // save new connection handle
  mobileCHandle = conn_handle;

  checkConnections();
}

void prph_disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.println("[Prph] Disconnected from Mobile");

  checkConnections();

  // make sure follower device is sent command to reset
  // to standy
  if(clientUart.discovered())
  {
    uint8_t command[4] = {67, 0, 255, 0};
    clientUart.write(command, 4);
  }
}

// Normally callbacks would be used instead of checking every loop
// void prph_bleuart_rx_callback(uint16_t conn_handle)
// {
//   (void) conn_handle;

//   if ( clientUart.discovered() )
//   {
//     clientUart.print(str, 20);
//   }
//   else
//   {
//     bleuart.println("[Prph] Central role not connected");
//   }
// }

/*------------------------------------------------------------------*/
/* Central
 *------------------------------------------------------------------*/
void scan_callback(ble_gap_evt_adv_report_t* report)
{
  // Since we configure the scanner with filterUuid()
  // Scan callback only invoked for device with bleuart service advertised  
  // Connect to the device with bleuart service in advertising packet  
  Bluefruit.Central.connect(report);
}

void cent_connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char peer_name[32] = { 0 };
  connection->getPeerName(peer_name, sizeof(peer_name));

  Serial.print("[Cent] Connected to ");
  Serial.printf("Handle: %i\n", conn_handle);
  Serial.println(peer_name);;

  if ( clientUart.discover(conn_handle) )
  {
    // Enable TXD's notify
    clientUart.enableTXD();
  }
  else
  {
    // disconnect since we couldn't find bleuart service
    Bluefruit.disconnect(conn_handle);
  }

  // save follower device handle
  followerCHandle = conn_handle;

  checkConnections();
}

void cent_disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
  
  Serial.println("[Cent] Disconnected");
  
  checkConnections();
}

/**
 * Callback invoked when uart received data
 * @param cent_uart Reference object to the service where the data 
 * arrived. In this example it is clientUart
 */
void cent_bleuart_rx_callback(BLEClientUart& cent_uart)
{
  char str[20+1] = { 0 };
  cent_uart.read(str, 20);
      
  Serial.print("[Cent] RX: ");
  Serial.println(str);

  if ( bleuart.notifyEnabled() )
  {
    // Forward data from our peripheral to Mobile
    bleuart.print( str );
  }
  else
  {
    // response with no prph message
    clientUart.println("[Cent] Peripheral role not connected");
  }  
}

// Process commands from Mobile App
void swapBuffers()
{
  uint8_t *base_addr = pixelBuffer;
  int pixelIndex = 0;
  for (int j = 0; j < height; j++)
  {
    for (int i = 0; i < width; i++) {
      if (components == 3) {
        neopixel.setPixelColor(pixelIndex, neopixel.Color(*base_addr, *(base_addr+1), *(base_addr+2)));
      }
      else {
        neopixel.setPixelColor(pixelIndex, neopixel.Color(*base_addr, *(base_addr+1), *(base_addr+2), *(base_addr+3) ));
      }
      base_addr+=components;
      pixelIndex++;
    }
    pixelIndex += stride - width;   // Move pixelIndex to the next row (take into account the stride)
  }
  
  neopixel.show();
}

void commandVersion() {
  Serial.println(F("Command: Version check"));
  sendResponse(NEOPIXEL_VERSION_STRING);
}

void commandSetup(const uint8_t* buffer) {
  Serial.println(F("Command: Setup"));

  // attributes for neopixel layout
  width = buffer[1];  
  height = buffer[2];  
  stride = buffer[3];
  componentsValue = buffer[4];
  is400Hz = buffer[5];

  neoPixelType pixelType;
  pixelType = componentsValue + (is400Hz ? NEO_KHZ400 : NEO_KHZ800);

  components = (componentsValue == NEO_RGB || componentsValue == NEO_RBG || componentsValue == NEO_GRB || componentsValue == NEO_GBR || componentsValue == NEO_BRG || componentsValue == NEO_BGR) ? 3:4;
  
  Serial.printf("\tsize: %dx%d\n", width, height);
  Serial.printf("\tstride: %d\n", stride);
  Serial.printf("\tpixelType %d\n", pixelType);
  Serial.printf("\tcomponents: %d\n", components);

  if (pixelBuffer != NULL) {
      delete[] pixelBuffer;
  }

  uint32_t size = width*height;
  pixelBuffer = new uint8_t[size*components];
  neopixel.updateLength(size);
  neopixel.updateType(pixelType);
  neopixel.setPin(PIN);

  // Done
  sendResponse("OK");
}

void commandSetBrightness(const uint8_t* buffer) {
  Serial.println(F("Command: SetBrightness"));

  // Read value
  uint8_t brightness = buffer[1];

  Serial.printf("\tBrightness is %d\n", brightness);

  // Set brightness
  neopixel.setBrightness(brightness);

  // Refresh pixels
  swapBuffers();

  // Done
  sendResponse("OK");
}

void commandClearColor(const uint8_t* buffer) {
  Serial.println(F("Command: ClearColor"));

  // Read color
  uint8_t color[MAXCOMPONENTS];
  for (int j = 0; j < components;) {
    if (bleuart.available()) {
      color[j] = buffer[j+1];
      j++;
    }
  }

  // Set all leds to color
  int size = width * height;
  uint8_t *base_addr = pixelBuffer;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < components; j++) {
      *base_addr = color[j];
      base_addr++;
    }
  }

  // Swap buffers
  Serial.println(F("ClearColor completed"));
  swapBuffers();


  if (components == 3) {
    Serial.printf("\tclear (%d, %d, %d)\n", color[0], color[1], color[2] );
  }
  else {
    Serial.printf("\tclear (%d, %d, %d, %d)\n", color[0], color[1], color[2], color[3] );
  }
  
  // Done
  sendResponse("OK");
}

void commandSetPixel(const uint8_t* buffer) {
  Serial.println(F("Command: SetPixel"));

  // Read position
  uint8_t x = buffer[1];
  uint8_t y = buffer[2];

  // Read colors
  uint32_t pixelOffset = y*width+x;
  uint32_t pixelDataOffset = pixelOffset*components;
  uint8_t *base_addr = pixelBuffer+pixelDataOffset;
  for (int j = 0; j < components;) {
    if (bleuart.available()) {
      *base_addr = buffer[j+2];
      base_addr++;
      j++;
    }
  }

  // Set colors
  uint32_t neopixelIndex = y*stride+x;
  uint8_t *pixelBufferPointer = pixelBuffer + pixelDataOffset;
  uint32_t color;
  if (components == 3) {
    color = neopixel.Color( *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2) );
    Serial.printf("\tcolor (%d, %d, %d)\n",*pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2) );
  }
  else {
    color = neopixel.Color( *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2), *(pixelBufferPointer+3) );
    Serial.printf("\tcolor (%d, %d, %d, %d)\n", *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2), *(pixelBufferPointer+3) );    
  }
  Serial.printf("\tindex: %d\n", neopixelIndex);
  neopixel.setPixelColor(neopixelIndex, color);
  neopixel.show();

  // Done
  sendResponse("OK");
}

// DO NOT USE THIS FOR NOW
void commandImage() {
  Serial.printf("Command: Image %dx%d, %d, %d\n", width, height, components, stride);
  
  // Receive new pixel buffer
  int size = width * height;
  uint8_t *base_addr = pixelBuffer;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < components;) {
      if (bleuart.available()) {
        *base_addr = bleuart.read();
        base_addr++;
        j++;
      }
    }

/*
    if (components == 3) {
      uint32_t index = i*components;
      Serial.printf("\tp%d (%d, %d, %d)\n", i, pixelBuffer[index], pixelBuffer[index+1], pixelBuffer[index+2] );
    }
    */
  }

  // Swap buffers
  Serial.println(F("Image received"));
  swapBuffers();

  // Done
  sendResponse("OK");
}

void sendResponse(char const *response) {
    Serial.printf("Send Response: %s\n", response);
    bleuart.write(response, strlen(response)*sizeof(char));
}

void checkConnections()
{
  bool mobileConnection = Bluefruit.connected(mobileCHandle);
  bool followerConnection = Bluefruit.connected(followerCHandle);

  Serial.printf("Mobile handle: %i\n", mobileCHandle);
  Serial.printf("Follower handle:%i\n", followerCHandle);

  Serial.printf("Mobile connection: %i\n", mobileConnection);
  Serial.printf("Follower connection: %i\n", followerConnection);

  if(mobileConnection && followerConnection)
    setGreen();
  else if(mobileConnection)
    setBlue();
  else if(followerConnection)
    setYellow();
  else
    setRed();
}

// Indicators for Status
void setRed()
{
  neopixel.setBrightness(255);
  neopixel.setPixelColor(0, 255, 0, 0);
  neopixel.show();
}

void setOrange()
{
  neopixel.setBrightness(255);
  neopixel.setPixelColor(0, 255, 165, 0);
  neopixel.show();
}

void setYellow()
{
  neopixel.setBrightness(255);
  neopixel.setPixelColor(0, 255, 255, 0);
  neopixel.show();
}

void setGreen()
{
  neopixel.setBrightness(255);
  neopixel.setPixelColor(0, 0, 255, 0);
  neopixel.show();
}

void setBlue()
{
  neopixel.setBrightness(255);
  neopixel.setPixelColor(0, 0, 0, 255);
  neopixel.show();
}
