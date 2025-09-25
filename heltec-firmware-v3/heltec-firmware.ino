// Prototype RadioDoge firmware for the Heltec WiFi LoRa 32 v2 & v3 modules
#include <Wire.h>
#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include "radioDogeTypes.h"
#include "Images/logoImage.h"
#include "Images/coin.h"
#include "Images/doge.h"
#include "Images/sendingDogeCoin.h"
#include "Images/receivingDogeCoin.h"

// V3 Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 21
#define SCREEN_ADDRESS 0x3C
#define SDA_PIN 17
#define SCL_PIN 18
#define VEXT_PIN 36  // Controls display power on V3
#define TEST_COIN_AMOUNT 1234.56

// WiFi Configuration
const char* ssid = "RadioDoge";  // Access Point name
const char* password = "radiodoge";  // AP password
WebServer server(80);  // Web server on port 80


// Global display object
Adafruit_SSD1306 radioDogeDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define RF_FREQUENCY 915000000   // Hz
#define TX_OUTPUT_POWER 5        // dBm
#define LORA_BANDWIDTH 0         // [0: 125 kHz, \
                                 //  1: 250 kHz, \
                                 //  2: 500 kHz, \
                                 //  3: Reserved]
#define LORA_SPREADING_FACTOR 7  // [SF7..SF12]
#define LORA_CODINGRATE 1        // [1: 4/5, \
                                 //  2: 4/6, \
                                 //  3: 4/7, \
                                 //  4: 4/8]
#define LORA_PREAMBLE_LENGTH 8   // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0    // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE 1000
#define SERIAL_HEADER_SIZE 2
#define BUFFER_SIZE 256  // Define the payload size here
#define CONTROL_SIZE 8
#define SERIAL_TERMINATOR 255
#define SENDER_ADDRESS_OFFSET 2
#define COMMAND_ACK_CODE 6 // 0x06 = 'ACK'
#define COMMAND_NACK_CODE 21 // 0x15 = 'NAK'
#define HOST_ACK_NACK_SIZE 3
#define FIRMWARE_VERSION 1

#ifdef WIFI_LoRa_32_V2
#define HELTEC_BOARD_VERSION 2
#else
#define HELTEC_BOARD_VERSION 3
#endif

char txPacket[BUFFER_SIZE];
char rxPacket[BUFFER_SIZE];
uint8_t controlPacket[CONTROL_SIZE];
uint8_t serialBuf[BUFFER_SIZE];
uint8_t hostReplyBuf[BUFFER_SIZE];
uint8_t serialHeader[SERIAL_HEADER_SIZE];
uint8_t hostCommandReply[BUFFER_SIZE];
uint8_t hostACK[3] = {RESULT_CODE, 1, COMMAND_ACK_CODE};
uint8_t hostNACK[3] = {RESULT_CODE, 1, COMMAND_NACK_CODE};

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t messageSize, int16_t rssiMeasured, int8_t snr);

int16_t rssi;
int16_t rxSize;
bool isLoRaIdle = true;
bool needToSendACK = false;

nodeAddress local;
nodeAddress dest;
nodeAddress senderAddress;

void setup() {
  Serial.begin(115200);
  
  // CRITICAL: Set Vext pin LOW FIRST for V3 display power
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100); // Give display time to power up
  
  // Initialize I2C with correct V3 pins
  Wire.begin(SDA_PIN, SCL_PIN);
  
  // Initialize display
  if(!radioDogeDisplay.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  Serial.println("Display initialized successfully!");
  
  Mcu.begin(HELTEC_BOARD_VERSION, 0);
  rssi = 0;

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.RxTimeout = OnRxTimeout;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

  delay(100);
  
  // Setup WiFi Access Point
  WiFi.softAP(ssid, password);
  Serial.println("WiFi AP started");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
  
  
  // Setup web server routes
  setupWebServer();
  
  // Show startup animations
  DisplayStartupSequence();
  
  Serial.println("RadioDoge initialized!");
  Serial.println("Connect to WiFi: RadioDoge");
  Serial.println("Password: radiodoge");
  Serial.println("Open browser: http://192.168.4.1");
  Serial.println("Use WiFi web interface for mobile access");
}

void loop() {
  // Handle web server requests
  server.handleClient();
  
  
  //RawSerialMessageSendAndReceive()
  CommandAndControlLoop();
}

// V3 Display Functions
void DisplayStartupSequence() {
  // Show some animations
  CoinAnimation();
  DogeAnimation();
  // Show the send and receive images
  DrawSendingCoinsImage(TEST_COIN_AMOUNT);
  delay(2500);
  DrawReceivingCoinsImage(TEST_COIN_AMOUNT);
  delay(2500);
  // Display RadioDoge Image on startup
  DrawRadioDogeLogo();
}

// Simple display functions for V3
void DisplayTXMessage(String message, nodeAddress dest) {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.setTextSize(1);
  radioDogeDisplay.setTextColor(SSD1306_WHITE);
  radioDogeDisplay.setCursor(0, 0);
  radioDogeDisplay.println("TX: " + message);
  radioDogeDisplay.setCursor(0, 20);
  radioDogeDisplay.println("To: " + String(dest.region) + "." + String(dest.community) + "." + String(dest.node));
  radioDogeDisplay.display();
}

void DisplayRXMessage(String message, nodeAddress sender) {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.setTextSize(1);
  radioDogeDisplay.setTextColor(SSD1306_WHITE);
  radioDogeDisplay.setCursor(0, 0);
  radioDogeDisplay.println("RX: " + message);
  radioDogeDisplay.setCursor(0, 20);
  radioDogeDisplay.println("From: " + String(sender.region) + "." + String(sender.community) + "." + String(sender.node));
  radioDogeDisplay.display();
}

void DisplayBroadcastMessage(String message, nodeAddress sender) {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.setTextSize(1);
  radioDogeDisplay.setTextColor(SSD1306_WHITE);
  radioDogeDisplay.setCursor(0, 0);
  radioDogeDisplay.println("BROADCAST: " + message);
  radioDogeDisplay.setCursor(0, 20);
  radioDogeDisplay.println("From: " + String(sender.region) + "." + String(sender.community) + "." + String(sender.node));
  radioDogeDisplay.display();
}

void DisplayCommandAndControl(uint8_t command) {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.setTextSize(1);
  radioDogeDisplay.setTextColor(SSD1306_WHITE);
  radioDogeDisplay.setCursor(0, 0);
  radioDogeDisplay.println("Command: " + String(command));
  radioDogeDisplay.display();
}

void DisplayLocalAddress(nodeAddress addr) {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.setTextSize(1);
  radioDogeDisplay.setTextColor(SSD1306_WHITE);
  radioDogeDisplay.setCursor(0, 0);
  radioDogeDisplay.println("Address: " + String(addr.region) + "." + String(addr.community) + "." + String(addr.node));
  radioDogeDisplay.display();
}

void DisplayHardwareInfo(int version) {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.setTextSize(1);
  radioDogeDisplay.setTextColor(SSD1306_WHITE);
  radioDogeDisplay.setCursor(0, 0);
  radioDogeDisplay.println("Heltec V" + String(version));
  radioDogeDisplay.setCursor(0, 20);
  radioDogeDisplay.println("RadioDoge");
  radioDogeDisplay.display();
}

void DisplayCustomStringMessage(String message, int yOffset) {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.setTextSize(1);
  radioDogeDisplay.setTextColor(SSD1306_WHITE);
  radioDogeDisplay.setCursor(0, yOffset);
  radioDogeDisplay.println(message);
  radioDogeDisplay.display();
}

void DrawRadioDogeLogo() {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.drawXBitmap(0, 0, logo_bits, logo_width, logo_height, SSD1306_WHITE);
  radioDogeDisplay.display();
}

void DogeAnimation() {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.drawXBitmap(32, 0, doge_bits, doge_width, doge_height, SSD1306_WHITE);
  radioDogeDisplay.display();
  delay(1000);
}

void CoinAnimation() {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.drawXBitmap(32, 0, coin_bits, coin_width, coin_height, SSD1306_WHITE);
  radioDogeDisplay.display();
  delay(1000);
}

void DrawSendingCoinsImage(float amount) {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.drawXBitmap(0, 0, sendCoin_bits, sendCoin_width, sendCoin_height, SSD1306_WHITE);
  radioDogeDisplay.display();
}

void DrawReceivingCoinsImage(float amount) {
  radioDogeDisplay.clearDisplay();
  radioDogeDisplay.drawXBitmap(0, 0, rcvCoin_bits, rcvCoin_width, rcvCoin_height, SSD1306_WHITE);
  radioDogeDisplay.display();
}

// Older test that allows for the sending of messages (specified from the host over serial) between devices without any addressing
void RawSerialMessageSendAndReceive() {
  if (isLoRaIdle) {
    isLoRaIdle = false;
    // Indicating that we are moving into rx mode (debug)
    //Serial.println("VERY RX");
    // Enter back into RX mode
    Radio.Rx(0);
  }

  // Check if there is data on the serial port to send out
  if (CreateMessage()) {
    isLoRaIdle = false;
    // Indicating that we are starting a TX
    //Serial.println("VERY TX");
    //Serial.printf("\r\nSending packet \"%X,\" , Length %d\r\n", txPacket, strlen(txPacket));
    Radio.Send((uint8_t *)txPacket, strlen(txPacket));
  }

  Radio.IrqProcess();
}

// Updated core functionality supporting addressing, pings, acks, and user defined messages
void CommandAndControlLoop() {
  if (needToSendACK) {
    // For now we will delay before sending an ACK (debug purposes)
    delay(2000);
    isLoRaIdle = false;
    needToSendACK = false;
    SendACK(dest);
  }

  if (isLoRaIdle) {
    isLoRaIdle = false;
    // Indicating that we are moving into rx mode (debug)
    //Serial.println("VERY RX");
    // Enter back into RX mode
    Radio.Rx(0);
  }

  // Check if there is data on the serial port to send out
  if (Serial.available() > 0) {
    HostSerialRead();
  }

  Radio.IrqProcess();
}

void OnTxDone(void) {
  // Indicate that TX is done (debug)
  //Serial.println("WOW MUCH TX");
  isLoRaIdle = true;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  // Indicate that TX failed (debug)
  //Serial.println("OOPS TX BAD");
  isLoRaIdle = true;
}

void OnRxTimeout(void) {
  Radio.Sleep();
  // Indicate that RX failed (debug)
  // Probably should not see this since RX should not timeout
  //Serial.println("OOPS RX BAD");
  isLoRaIdle = true;
}

// Callback function run when a packet is received by the LoRa module
void OnRxDone(uint8_t *payload, uint16_t messageSize, int16_t rssiMeasured, int8_t snr) {
  rssi = rssiMeasured;
  rxSize = messageSize;
  memcpy(rxPacket, payload, messageSize);
  rxPacket[messageSize] = '\0';
  Radio.Sleep();
  // Indicate we received a packet (debug)
  //Serial.println("WOW MUCH RX");
  //Serial.printf("\r\nReceived packet! Rssi %d , Length %d\r\n", rssi, rxSize);
  ParseReceivedMessage();
  isLoRaIdle = true;
}

// Create a custom message that is specified by the host over the serial port
bool CreateMessage() {
  if (Serial.available() > 0) {
    String readString = Serial.readStringUntil('\0');
    // Echo back the read string (debug)
    //Serial.printf("REPLY: %s\n", readString);
    // Display on the screen
    DisplayTXMessage(readString, dest);
    readString.toCharArray(txPacket, BUFFER_SIZE);
    return true;
  }
  return false;
}

// Prepopulates control message buffers with local address
void InitControlMessages() {
  controlPacket[0] = (uint8_t)UNKNOWN;
  // Payload size for control packets is fixed at 6
  controlPacket[1] = 6;
  controlPacket[2] = local.region;
  controlPacket[3] = local.community;
  controlPacket[4] = local.node;
}

// Set the address of this device
void SetLocalAddressFromSerialBuffer(int offset) {
  local.region = serialBuf[offset];
  local.community = serialBuf[offset + 1];
  local.node = serialBuf[offset + 2];
}

// Set the destination address for the next transmission
void SetDestinationFromSerialBuffer(int offset) {
  dest.region = serialBuf[offset];
  dest.community = serialBuf[offset + 1];
  dest.node = serialBuf[offset + 2];
}

// Set the destination to the global broadcast / reserved address for the next transmission
void SetDestinationAsBroadcast()
{
  dest.region = 255;
  dest.community = 255;
  dest.node = 255;
}

// Extracts the address of the sending node from the received packet buffer
void SetSenderAddress()
{
  senderAddress.region = rxPacket[SENDER_ADDRESS_OFFSET];
  senderAddress.community = rxPacket[SENDER_ADDRESS_OFFSET + 1];
  senderAddress.node = rxPacket[SENDER_ADDRESS_OFFSET + 2];
}

// Retrieve the local address
void GetLocalAddress() {
  hostCommandReply[1] = 3;
  hostCommandReply[2] = local.region;
  hostCommandReply[3] = local.community;
  hostCommandReply[4] = local.node;
  Serial.write(hostCommandReply, 5);
  //Serial.printf("MUCH LCL: %d.%d.%d\n", local.region, local.community, local.node);
}

// Indicate to the host that an ACK was received and display it on the screen
void ReceivedACK() {
  SetSenderAddress();
  DisplayRXMessage("ACK", senderAddress);
  //Serial.printf("MUCH ACK FR %d.%d.%d\n", senderAddress.region, senderAddress.community, senderAddress.node);
  // @TODO indicate to host we received an ACK
}

// Indicate to the host that a ping was received and display it on the screen
void ReceivedPing() {
  SetSenderAddress();
  DisplayRXMessage("Ping!", senderAddress);
  //Serial.printf("MUCH PING FR %d.%d.%d\n", senderAddress.region, senderAddress.community, senderAddress.node);
  // @TODO indicate to host we received a Ping
}

// Send a ping to the specified destination address
void SendPing(nodeAddress destination) {
  // Message structure will by [message type, sender, destination]
  controlPacket[0] = (uint8_t)PING;
  controlPacket[5] = destination.region;
  controlPacket[6] = destination.community;
  controlPacket[7] = destination.node;
  isLoRaIdle = false;
  DisplayTXMessage("Ping", destination);
  Radio.Send(controlPacket, CONTROL_SIZE);
  //Serial.printf("Sending Ping to %d.%d.%d\n", destination.region, destination.community, destination.node);
  Serial.write(hostACK, HOST_ACK_NACK_SIZE);
}

// Send an ACK to the specified destination address
void SendACK(nodeAddress destination) {
  // Message structure will by [message type, sender, destination]
  controlPacket[0] = (uint8_t)ACK;
  controlPacket[5] = destination.region;
  controlPacket[6] = destination.community;
  controlPacket[7] = destination.node;
  DisplayTXMessage("ACK", destination);
  isLoRaIdle = false;
  Radio.Send(controlPacket, CONTROL_SIZE);
  //Serial.printf("Sending ACK to %d.%d.%d\n", destination.region, destination.community, destination.node);
  Serial.write(hostACK, HOST_ACK_NACK_SIZE);
}

// Send a message to the specified destination
void SendMessage(nodeAddress destination, String message, String type) {
  dest = destination;
  
  // Prepare message data
  String messageData = type + ":" + message;
  int messageLength = messageData.length();
  
  if (messageLength > 255) {
    messageLength = 255;
  }
  
  // Copy message to serial buffer
  messageData.getBytes(serialBuf, messageLength + 1);
  
  // Display message on display
  DisplayTXMessage(message, dest);
  
  // Update packet header and send the message over the air
  serialBuf[0] = (uint8_t)MESSAGE;
  Radio.Send(serialBuf, (uint8_t)messageLength);
}

// Send a transaction to the specified destination
void SendTransaction(nodeAddress destination, String transaction, String type) {
  dest = destination;
  
  // Prepare transaction data
  String txData = type + ":" + transaction;
  int txLength = txData.length();
  
  if (txLength > 255) {
    txLength = 255;
  }
  
  // Copy transaction to serial buffer
  txData.getBytes(serialBuf, txLength + 1);
  
  // Display transaction on display
  DisplayTXMessage("TX: " + type, dest);
  
  // Update packet header and send the transaction over the air
  serialBuf[0] = (uint8_t)TRANSACTION;
  Radio.Send(serialBuf, (uint8_t)txLength);
}

// Send a broadcast message
void SendBroadcast(String message, String type, String priority) {
  // Prepare broadcast data
  String broadcastData = type + ":" + priority + ":" + message;
  int broadcastLength = broadcastData.length();
  
  if (broadcastLength > 255) {
    broadcastLength = 255;
  }
  
  // Copy broadcast to serial buffer
  broadcastData.getBytes(serialBuf, broadcastLength + 1);
  
  // Display broadcast on display
  DisplayBroadcastMessage(message, local);
  
  // Update packet header and send the broadcast over the air
  serialBuf[0] = (uint8_t)BROADCAST;
  Radio.Send(serialBuf, (uint8_t)broadcastLength);
}

// Basically we will just send out the the serial buffer
// Change up the first byte to indicate messageType=MESSAGE
void SendMessageFromBuffer(int messageLength) {
  // For now we will cap the size of the message we can send
  // Will have to investigate breaking up large messages in future but Radio.Send only accepts a uint8_t as the buffer size
  if (messageLength > 255) {
    messageLength = 255;
  }

  // Display message on display
  String messageString = ExtractStringMessageFromBuffer(serialBuf, messageLength);
  DisplayTXMessage(messageString, dest);

  // Update packet header and send the message over the air
  serialBuf[0] = (uint8_t)MESSAGE;
  Radio.Send(serialBuf, (uint8_t)messageLength);
}

// Read the serial header and extract the command type and payload size from it.
// Currently the header consists of 2 bytes: the command type and the payload size.
bool ReadSerialHeader(serialCommand &commandType, uint8_t &payloadSize) {
  size_t numRead = Serial.readBytes(serialHeader, SERIAL_HEADER_SIZE);
  if (numRead != SERIAL_HEADER_SIZE) {
    return false;
  }
  // First byte will be command
  // Next byte will be payload size
  commandType = (serialCommand)serialHeader[0];
  payloadSize = serialHeader[1];
  return true;
}

// Read the host payload into the serial buffer.
bool ReadSerialPayload(uint8_t payloadSize) {
  uint8_t bytesRead = Serial.readBytes(serialBuf, payloadSize);
  if (bytesRead != payloadSize) {
    return false;
  }
  //Serial.printf("Read %d bytes\n", bytesRead);
  return true;
}

// Read serial data from the host and perform the specified command/control function. 
// This function expects the host to send a header that is 8 bytes in length and then a payload that can range from 0-255 bytes.
// The header contains the command type and payload size information (see ReadSerialHeader for more info on the header)
void HostSerialRead() {
  serialCommand commandVal;
  uint8_t payloadSize;
  bool headerSuccess = ReadSerialHeader(commandVal, payloadSize);
  if (!headerSuccess) {
    return;
  }
  hostCommandReply[0] = commandVal;
  //Serial.printf("Command From Host Received: %d\n", commandVal);
  // Display the command on the module screen for now and wait for debug purposes
  DisplayCommandAndControl(commandVal);
  // Now we will read in the host's payload
  bool payloadSuccess = ReadSerialPayload(payloadSize);
  if (!payloadSuccess) {
    Serial.write(hostNACK, HOST_ACK_NACK_SIZE);
    return;
  }
  delay(500);
  switch (commandVal) {
    case ADDRESS_GET:
      GetLocalAddress();
      DisplayLocalAddress(local);
      break;
    case ADDRESS_SET:
      SetLocalAddressFromSerialBuffer(0);
      InitControlMessages();
      DisplayLocalAddress(local);
      Serial.write(hostACK, HOST_ACK_NACK_SIZE);
      break;
    case PING_REQUEST:
      SetDestinationFromSerialBuffer(0);
      SendPing(dest);
      break;
    case MESSAGE_REQUEST:
      SetDestinationFromSerialBuffer(4);
      SendMessageFromBuffer(payloadSize);
      break;
    case HARDWARE_INFO:
      DisplayHardwareInfo(HELTEC_BOARD_VERSION);
      //Serial.printf("RD HT V%d FW01\n", HELTEC_BOARD_VERSION);
      SendHardwareInfoToHost();
      break;
    case DISPLAY_CONTROL:
      ProcessDisplayControl(payloadSize);
      Serial.write(hostACK, HOST_ACK_NACK_SIZE);
      break;
    case HOST_FORMED_PACKET:
      //ParseHostFormedPacket(payloadSize);
      SetDestinationFromSerialBuffer(5);
      DisplayTXMessage("Custom Packet!", dest);
      // Send out the host formed packet
      Radio.Send(serialBuf, payloadSize);
      // Acknowledge the host that we sent the packet
      Serial.write(hostACK, HOST_ACK_NACK_SIZE);
      break;
    case MULTIPART_PACKET:
      SetDestinationFromSerialBuffer(5);
      char tempBuf[64];
      sprintf(tempBuf, "Part %i of %i", serialBuf[10], serialBuf[11]);
      DisplayTXMessage(String(tempBuf), dest);
      Radio.Send(serialBuf, payloadSize);
      // Send ACK to let them know we sent out that part
      Serial.write(hostACK, HOST_ACK_NACK_SIZE);
      break;
    default:
      // Indicate that command was not understood (Send NACK)
      Serial.write(hostNACK, HOST_ACK_NACK_SIZE);
      break;
  }
}

void ProcessCoinDisplay(int payloadSize)
{
  if (payloadSize < 5)
  {
    return;
  }
  float coinAmount = 0;
  memcpy(&coinAmount, serialBuf + 1, sizeof(coinAmount));
  if (serialBuf[0] == RECEIVING_DISPLAY)
  {
    DrawReceivingCoinsImage(coinAmount);
  }
  else if (serialBuf[0] == SENDING_DISPLAY)
  {
    DrawSendingCoinsImage(coinAmount);
  }
}

void ProcessDisplayControl(int payloadSize)
{
  // First byte of the payload/serial buffer will indicate what to display
  switch(serialBuf[0])
  {
    case STRING_DISPLAY:
    {
      int yOffset = serialBuf[1];
      int stringLength = payloadSize - 2;
      char *extractedString = new char[stringLength + 1];
      for (int i = 0; i < stringLength; i++) 
      {
        extractedString[i] = (char)serialBuf[i+2];
      }
      extractedString[stringLength] = '\0';
      String displayString(extractedString);
      DisplayCustomStringMessage(displayString, yOffset);
      free(extractedString);
    }
    break;
    case LOGO_DISPLAY:
    DrawRadioDogeLogo();
    break;
    case DOGE_ANIMATION_DISPLAY:
    DogeAnimation();
    break;
    case COIN_ANIMATION_DISPLAY:
    CoinAnimation();
    break;
    case RECEIVING_DISPLAY:
    ProcessCoinDisplay(payloadSize);
    break;
    case SENDING_DISPLAY:
    ProcessCoinDisplay(payloadSize);
    break;
    default:
    // @TODO
    break;
  }
}

void SendHardwareInfoToHost()
{
  // Just send 'h' for heltec, board version, and firmware version for now
  uint8_t reply[5] = {HARDWARE_INFO, 3, 'h', HELTEC_BOARD_VERSION, FIRMWARE_VERSION}; 
  Serial.write(reply, 5);
}

void ParseHostFormedPacket(uint8_t payloadSize) {
  // Echo back information about what was read...
  char *extractedMessage = new char[payloadSize + 1];
  for (int i = 0; i < payloadSize; i++) {
    extractedMessage[i] = (char)serialBuf[i];
  }
  extractedMessage[payloadSize] = '\0';
  String messageString(extractedMessage);
  free(extractedMessage);
  Serial.println(messageString);
}

// Parse a LoRa message received over the air from another module
void ParseReceivedMessage() {
  if (CheckIfPacketForMe()) {
    //Serial.println("PACKET FOR ME");
    messageType mType = (messageType)rxPacket[0];
    switch (mType) {
      case ACK:
        ReceivedACK();
        break;
      case PING:
        ReceivedPing();
        // Set the ACK destination as the address of the sender
        dest.region = rxPacket[2];
        dest.community = rxPacket[3];
        dest.node = rxPacket[4];
        // For some reason sending the ACK here works on V2 but not V3
        // Not sure why but sending it in the main loop works for both so we will do it there
        // We will just set a flag to send an ACK
        needToSendACK = true;
        break;
      case MESSAGE:
        {
          String messageString = ExtractStringMessageFromBuffer((uint8_t *)rxPacket, rxSize);
          SetSenderAddress();
          DisplayRXMessage(messageString, senderAddress);
          //Serial.printf("MUCH TLK FR %d.%d.%d:\n", senderAddress.region, senderAddress.community, senderAddress.node);
          //Serial.println(messageString);
        }
        break;
      case TRANSACTION:
        {
          String transactionString = ExtractStringMessageFromBuffer((uint8_t *)rxPacket, rxSize);
          SetSenderAddress();
          DisplayRXMessage("TX: " + transactionString, senderAddress);
          //Serial.printf("TRANSACTION FR %d.%d.%d:\n", senderAddress.region, senderAddress.community, senderAddress.node);
          //Serial.println(transactionString);
        }
        break;
      case BROADCAST:
        {
          String broadcastString = ExtractStringMessageFromBuffer((uint8_t *)rxPacket, rxSize);
          SetSenderAddress();
          DisplayBroadcastMessage(broadcastString, senderAddress);
          //Serial.printf("BROADCAST FR %d.%d.%d:\n", senderAddress.region, senderAddress.community, senderAddress.node);
          //Serial.println(broadcastString);
        }
        break;
      case HOST_FORMED_PACKET:
        SetSenderAddress();
        DisplayRXMessage("Packet Received!", senderAddress);
        // We will just pass on the message directly to the host
        Serial.write(rxPacket, rxSize);
        break;
      case MULTIPART_PACKET:
        SetSenderAddress();
        char tempBuf[64];
        sprintf(tempBuf, "Packet part %i of %i", rxPacket[10], rxPacket[11]);
        DisplayRXMessage(String(tempBuf), senderAddress);
        Serial.write(rxPacket, rxSize);
        break;
      default:
        // (debug)
        //Serial.println("wat rcv?");
        break;
    }
  } 
  else if (CheckIfPacketIsGlobalBroadcast()){
    // Now that we know that the packet is a global broadcast...
    SetSenderAddress();
    DisplayBroadcastMessage("Broadcast Received!", senderAddress);
    // Check broadcast type
    // @TODO
    // We will just pass on the broadcast message directly to the host
    Serial.write(rxPacket, rxSize);
  }
  else {
    //Serial.printf("NOT FR ME: FR %d.%d.%d\n", rxPacket[4], rxPacket[5], rxPacket[6]);
  }
}

// Checks to see if the received packet's destination is the same as the local address
bool CheckIfPacketForMe() {
  return (local.region == rxPacket[5]) && (local.community == rxPacket[6]) && (local.node == rxPacket[7]);
}

// Checks to see if the received packet's destination is intended for every listening node
bool CheckIfPacketIsGlobalBroadcast()
{
  return (255 == rxPacket[5]) && (255 == rxPacket[6]) && (255 == rxPacket[7]);
}

// Extract the payload message from the given buffer (assists with displaying on screen)
String ExtractStringMessageFromBuffer(uint8_t *buf, int bufferSize) {
  int messageSize = bufferSize - 6;
  char *extractedMessage = new char[messageSize];
  for (int i = 0; i < messageSize; i++) {
    extractedMessage[i] = (char)buf[i + 7];
  }
  String messageString(extractedMessage);
  free(extractedMessage);
  return messageString;
}

// Web Server Functions
void setupWebServer() {
  // Serve the main control page
  server.on("/", handleRoot);
  
  // API endpoints (legacy text format)
  server.on("/ping", handlePing);
  server.on("/message", handleMessage);
  server.on("/transaction", handleTransaction);
  server.on("/broadcast", handleBroadcast);
  server.on("/ack", handleACK);
  server.on("/address", handleAddress);
  server.on("/status", handleStatus);
  
  // REST API endpoints with JSON support
  server.on("/api/ping", HTTP_GET, handleApiPing);
  server.on("/api/ping", HTTP_POST, handleApiPing);
  server.on("/api/message", HTTP_GET, handleApiMessage);
  server.on("/api/message", HTTP_POST, handleApiMessage);
  server.on("/api/transaction", HTTP_GET, handleApiTransaction);
  server.on("/api/transaction", HTTP_POST, handleApiTransaction);
  server.on("/api/broadcast", HTTP_GET, handleApiBroadcast);
  server.on("/api/broadcast", HTTP_POST, handleApiBroadcast);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/address", HTTP_GET, handleApiAddress);
  server.on("/api/address", HTTP_POST, handleApiAddress);
  
  server.begin();
  Serial.println("Web server started");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>RadioDoge Control</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:'Comic Sans MS','Segoe UI',Tahoma,Geneva,Verdana,sans-serif;margin:0;background:#121212;color:#ffffff;min-height:100vh;line-height:1.6;}";
  html += ".container{max-width:900px;margin:0 auto;background:#1a1a1a;padding:30px;border-radius:20px;box-shadow:0 20px 40px rgba(0,0,0,0.5);margin-top:20px;margin-bottom:20px;}";
  html += "h1{color:#ffc107;text-align:center;margin-bottom:40px;font-size:3em;font-weight:700;text-shadow:0 0 20px rgba(255,193,7,0.3);letter-spacing:2px;display:flex;align-items:center;justify-content:center;gap:20px;}";
  html += ".title-icon{width:60px;height:60px;fill:#ffc107;filter:drop-shadow(0 0 10px rgba(255,193,7,0.5));}";
  html += ".signal-wave{animation:transmit 2s ease-in-out infinite;}";
  html += ".signal-wave:nth-child(2){animation-delay:0.3s;}";
  html += ".signal-wave:nth-child(3){animation-delay:0.6s;}";
  html += ".signal-wave:nth-child(4){animation-delay:0.9s;}";
  html += "@keyframes transmit{0%,100%{opacity:0;transform:scale(0.5);}50%{opacity:1;transform:scale(1);}}";
  html += "h2{color:#ffc107;border-bottom:3px solid #ffc107;padding-bottom:15px;margin-top:40px;font-size:1.8em;font-weight:600;text-transform:uppercase;letter-spacing:1px;}";
  html += "h3{color:#ffc107;margin:20px 0 15px 0;font-size:1.3em;font-weight:600;}";
  html += ".button{background:linear-gradient(135deg,#ffc107 0%,#ffb300 100%);color:#121212;border:none;padding:15px 30px;margin:8px 0;border-radius:12px;cursor:pointer;font-size:16px;font-weight:700;transition:all 0.3s ease;text-transform:uppercase;letter-spacing:1px;box-shadow:0 4px 15px rgba(255,193,7,0.3);width:100%;box-sizing:border-box;}";
  html += ".button:hover{background:linear-gradient(135deg,#ffb300 0%,#ffa000 100%);transform:translateY(-3px);box-shadow:0 8px 25px rgba(255,193,7,0.4);}";
  html += ".button:active{transform:translateY(-1px);}";
  html += ".button.danger{background:linear-gradient(135deg,#dc3545 0%,#c82333 100%);box-shadow:0 4px 15px rgba(220,53,69,0.3);}";
  html += ".button.danger:hover{background:linear-gradient(135deg,#c82333 0%,#bd2130 100%);box-shadow:0 8px 25px rgba(220,53,69,0.4);}";
  html += ".button.success{background:linear-gradient(135deg,#28a745 0%,#218838 100%);box-shadow:0 4px 15px rgba(40,167,69,0.3);}";
  html += ".button.success:hover{background:linear-gradient(135deg,#218838 0%,#1e7e34 100%);box-shadow:0 8px 25px rgba(40,167,69,0.4);}";
  html += "input[type=text],input[type=number],textarea,select{width:100%;padding:15px;margin:8px 0;border:2px solid #333;border-radius:12px;font-size:16px;box-sizing:border-box;background:#2a2a2a;color:#ffffff;transition:all 0.3s ease;}";
  html += "input[type=text]:focus,input[type=number]:focus,textarea:focus,select:focus{border-color:#ffc107;outline:none;box-shadow:0 0 15px rgba(255,193,7,0.2);background:#333;}";
  html += "input[type=text]::placeholder,input[type=number]::placeholder,textarea::placeholder{color:#888;}";
  html += ".status{background:linear-gradient(135deg,#2a2a2a 0%,#333 100%);padding:20px;border-radius:15px;margin:20px 0;border-left:5px solid #ffc107;box-shadow:0 5px 15px rgba(0,0,0,0.3);}";
  html += ".section{background:linear-gradient(135deg,#1e1e1e 0%,#2a2a2a 100%);padding:25px;margin:20px 0;border-radius:15px;border:1px solid #333;box-shadow:0 5px 15px rgba(0,0,0,0.2);transition:all 0.3s ease;}";
  html += ".section:hover{transform:translateY(-2px);box-shadow:0 8px 25px rgba(0,0,0,0.3);}";
  html += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:25px;margin:25px 0;}";
  html += ".response{background:#1a1a1a;padding:20px;border-radius:12px;margin:20px 0;border-left:5px solid #ffc107;font-family:'Courier New',monospace;white-space:pre-wrap;color:#ffc107;box-shadow:0 5px 15px rgba(0,0,0,0.3);}";
  html += ".address-display{background:linear-gradient(135deg,#ffc107 0%,#ffb300 100%);color:#121212;padding:15px;border-radius:10px;margin:10px 0;font-weight:700;text-align:center;box-shadow:0 4px 15px rgba(255,193,7,0.3);}";
  html += "label{color:#ffc107;font-weight:600;margin-bottom:8px;display:block;text-transform:uppercase;letter-spacing:1px;font-size:0.9em;}";
  html += "p{color:#ccc;margin:10px 0;}";
  html += ".button-group{margin:15px 0;}";
  html += ".accordion{background:linear-gradient(135deg,#1e1e1e 0%,#2a2a2a 100%);border:1px solid #333;border-radius:15px;margin:20px 0;overflow:hidden;box-shadow:0 5px 15px rgba(0,0,0,0.2);}";
  html += ".accordion-header{background:linear-gradient(135deg,#ffc107 0%,#ffb300 100%);color:#121212;padding:20px 25px;cursor:pointer;font-size:1.4em;font-weight:700;text-transform:uppercase;letter-spacing:1px;transition:all 0.3s ease;display:flex;justify-content:space-between;align-items:center;}";
  html += ".accordion-header:hover{background:linear-gradient(135deg,#ffb300 0%,#ffa000 100%);}";
  html += ".accordion-header.active{background:linear-gradient(135deg,#ffa000 0%,#ff8f00 100%);}";
  html += ".accordion-content{padding:0;max-height:0;overflow:hidden;transition:max-height 0.3s ease;background:#1a1a1a;}";
  html += ".accordion-content.active{max-height:1000px;padding:25px;}";
  html += ".accordion-icon{width:20px;height:20px;transition:transform 0.3s ease;fill:#121212;}";
  html += ".accordion-icon.rotated{transform:rotate(180deg);}";
  html += ".status{background:linear-gradient(135deg,#2a2a2a 0%,#333 100%);padding:20px;border-radius:15px;margin:20px 0;border-left:5px solid #ffc107;box-shadow:0 5px 15px rgba(0,0,0,0.3);text-align:center;}";
  html += "@media (max-width: 768px) {.grid{grid-template-columns:1fr;} .container{padding:20px;margin:10px;} h1{font-size:2.2em;flex-direction:column;gap:10px;} .title-icon{width:50px;height:50px;} .button{padding:12px 20px;font-size:14px;} .accordion-header{font-size:1.2em;padding:15px 20px;}}";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h1>";
  html += "<svg class='title-icon' viewBox='0 0 100 100'>";
  html += "<defs>";
  html += "<linearGradient id='towerGradient' x1='0%' y1='0%' x2='100%' y2='100%'>";
  html += "<stop offset='0%' style='stop-color:#ffc107;stop-opacity:1' />";
  html += "<stop offset='100%' style='stop-color:#ffb300;stop-opacity:1' />";
  html += "</linearGradient>";
  html += "</defs>";
  html += "<!-- Radio Tower Base -->";
  html += "<rect x='45' y='70' width='10' height='25' fill='url(#towerGradient)' rx='2'/>";
  html += "<!-- Tower Mast -->";
  html += "<rect x='48' y='20' width='4' height='50' fill='url(#towerGradient)' rx='1'/>";
  html += "<!-- Antenna Array -->";
  html += "<rect x='35' y='15' width='30' height='3' fill='url(#towerGradient)' rx='1'/>";
  html += "<rect x='40' y='12' width='20' height='3' fill='url(#towerGradient)' rx='1'/>";
  html += "<rect x='45' y='9' width='10' height='3' fill='url(#towerGradient)' rx='1'/>";
  html += "<!-- Center Antenna -->";
  html += "<rect x='49' y='5' width='2' height='10' fill='url(#towerGradient)' rx='1'/>";
  html += "<!-- Transmission Signals -->";
  html += "<circle cx='50' cy='50' r='15' stroke='#ffc107' stroke-width='2' fill='none' opacity='0.3' class='signal-wave'/>";
  html += "<circle cx='50' cy='50' r='25' stroke='#ffc107' stroke-width='2' fill='none' opacity='0.2' class='signal-wave'/>";
  html += "<circle cx='50' cy='50' r='35' stroke='#ffc107' stroke-width='2' fill='none' opacity='0.1' class='signal-wave'/>";
  html += "<circle cx='50' cy='50' r='45' stroke='#ffc107' stroke-width='2' fill='none' opacity='0.05' class='signal-wave'/>";
  html += "</svg>";
  html += "RadioDoge";
  html += "</h1>";
  
  // Device Status
  html += "<div class='status'>";
  html += "<h3>DEVICE STATUS</h3>";
  html += "<p><strong>Address:</strong> <span class='address-display'>" + String(local.region) + "." + String(local.community) + "." + String(local.node) + "</span></p>";
  html += "<p><strong>Status:</strong> Ready | <strong>LoRa:</strong> Active | <strong>WiFi:</strong> " + String(ssid) + "</p>";
  html += "</div>";
  
  
  // Transaction Section
  html += "<div class='accordion'>";
  html += "<div class='accordion-header' onclick='toggleAccordion(\"transaction\")'>";
  html += "<span>DOGECOIN TRANSACTION</span>";
  html += "<svg class='accordion-icon' id='transaction-icon' viewBox='0 0 24 24'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z'/></svg>";
  html += "</div>";
  html += "<div class='accordion-content' id='transaction-content'>";
  html += "<p>Send signed Dogecoin transaction via LoRa</p>";
  html += "<div class='grid'>";
  html += "<div>";
  html += "<label>Target Address</label>";
  html += "<input type='text' id='txAddress' placeholder='10.1.2' value='10.1.2'>";
  html += "</div>";
  html += "<div>";
  html += "<label>Transaction Type</label>";
  html += "<select id='txType'>";
  html += "<option value='signed'>Signed Transaction</option>";
  html += "<option value='raw'>Raw Transaction</option>";
  html += "<option value='utxo'>UTXO Data</option>";
  html += "</select>";
  html += "</div>";
  html += "</div>";
  html += "<textarea id='transaction' placeholder='Paste your signed Dogecoin transaction here...' rows='4'></textarea>";
  html += "<div class='button-group'>";
  html += "<button class='button success' onclick='sendTransaction()'>SEND TRANSACTION</button>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  // Broadcast Section
  html += "<div class='accordion'>";
  html += "<div class='accordion-header' onclick='toggleAccordion(\"broadcast\")'>";
  html += "<span>BROADCAST MESSAGE</span>";
  html += "<svg class='accordion-icon' id='broadcast-icon' viewBox='0 0 24 24'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z'/></svg>";
  html += "</div>";
  html += "<div class='accordion-content' id='broadcast-content'>";
  html += "<p>Send message to all devices in range</p>";
  html += "<div class='grid'>";
  html += "<div>";
  html += "<label>Broadcast Type</label>";
  html += "<select id='broadcastType'>";
  html += "<option value='announcement'>Announcement</option>";
  html += "<option value='emergency'>Emergency</option>";
  html += "<option value='network'>Network Update</option>";
  html += "<option value='transaction'>Transaction Broadcast</option>";
  html += "</select>";
  html += "</div>";
  html += "<div>";
  html += "<label>Priority</label>";
  html += "<select id='broadcastPriority'>";
  html += "<option value='normal'>Normal</option>";
  html += "<option value='high'>High</option>";
  html += "<option value='urgent'>Urgent</option>";
  html += "</select>";
  html += "</div>";
  html += "</div>";
  html += "<textarea id='broadcastMessage' placeholder='Enter broadcast message...' rows='3'></textarea>";
  html += "<div class='button-group'>";
  html += "<button class='button' onclick='sendBroadcast()'>BROADCAST TO ALL</button>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  // ACK Section
  html += "<div class='accordion'>";
  html += "<div class='accordion-header' onclick='toggleAccordion(\"ack\")'>";
  html += "<span>ACKNOWLEDGMENT</span>";
  html += "<svg class='accordion-icon' id='ack-icon' viewBox='0 0 24 24'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z'/></svg>";
  html += "</div>";
  html += "<div class='accordion-content' id='ack-content'>";
  html += "<p>Send ACK for received messages</p>";
  html += "<div class='grid'>";
  html += "<div>";
  html += "<label>ACK Target</label>";
  html += "<input type='text' id='ackAddress' placeholder='10.1.2' value='10.1.2'>";
  html += "</div>";
  html += "<div>";
  html += "<label>ACK Type</label>";
  html += "<select id='ackType'>";
  html += "<option value='received'>Message Received</option>";
  html += "<option value='processed'>Message Processed</option>";
  html += "<option value='error'>Error Response</option>";
  html += "</select>";
  html += "</div>";
  html += "</div>";
  html += "<div class='button-group'>";
  html += "<button class='button' onclick='sendACK()'>SEND ACK</button>";
  html += "</div>";
  html += "</div>";
  html += "</div>";

   // Ping Section
   html += "<div class='accordion'>";
   html += "<div class='accordion-header' onclick='toggleAccordion(\"ping\")'>";
   html += "<span>PING TEST</span>";
   html += "<svg class='accordion-icon' id='ping-icon' viewBox='0 0 24 24'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z'/></svg>";
   html += "</div>";
   html += "<div class='accordion-content' id='ping-content'>";
   html += "<p>Test connectivity between devices</p>";
   html += "<div class='grid'>";
   html += "<div>";
   html += "<label>Target Region</label>";
   html += "<input type='number' id='pingRegion' placeholder='10' min='0' max='255' value='10'>";
   html += "</div>";
   html += "<div>";
   html += "<label>Target Community</label>";
   html += "<input type='number' id='pingCommunity' placeholder='1' min='0' max='255' value='1'>";
   html += "</div>";
   html += "</div>";
   html += "<div class='grid'>";
   html += "<div>";
   html += "<label>Target Node</label>";
   html += "<input type='number' id='pingNode' placeholder='2' min='0' max='255' value='2'>";
   html += "</div>";
   html += "<div class='button-group'>";
   html += "<button class='button' onclick='sendPing()'>SEND PING</button>";
   html += "<button class='button' onclick='pingAll()'>PING ALL</button>";
   html += "</div>";
   html += "</div>";
   html += "</div>";
   html += "</div>";
   
   // Message Section
   html += "<div class='accordion'>";
   html += "<div class='accordion-header' onclick='toggleAccordion(\"message\")'>";
   html += "<span>SEND MESSAGE</span>";
   html += "<svg class='accordion-icon' id='message-icon' viewBox='0 0 24 24'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z'/></svg>";
   html += "</div>";
   html += "<div class='accordion-content' id='message-content'>";
   html += "<p>Send text/data transmission via LoRa</p>";
   html += "<div class='grid'>";
   html += "<div>";
   html += "<label>Target Address</label>";
   html += "<input type='text' id='msgAddress' placeholder='10.1.2' value='10.1.2'>";
   html += "</div>";
   html += "<div>";
   html += "<label>Message Type</label>";
   html += "<select id='msgType'>";
   html += "<option value='text'>Text Message</option>";
   html += "<option value='data'>Data Packet</option>";
   html += "<option value='command'>Command</option>";
   html += "</select>";
   html += "</div>";
   html += "</div>";
   html += "<textarea id='message' placeholder='Enter your message here...' rows='3'></textarea>";
   html += "<div class='button-group'>";
   html += "<button class='button' onclick='sendMessage()'>SEND MESSAGE</button>";
   html += "</div>";
   html += "</div>";
   html += "</div>";
  
  // Device Configuration
  html += "<div class='accordion'>";
  html += "<div class='accordion-header' onclick='toggleAccordion(\"config\")'>";
  html += "<span>DEVICE CONFIGURATION</span>";
  html += "<svg class='accordion-icon' id='config-icon' viewBox='0 0 24 24'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z'/></svg>";
  html += "</div>";
  html += "<div class='accordion-content' id='config-content'>";
  html += "<div class='grid'>";
  html += "<div>";
  html += "<label>Region (0-255)</label>";
  html += "<input type='number' id='region' placeholder='10' min='0' max='255' value='" + String(local.region) + "'>";
  html += "</div>";
  html += "<div>";
  html += "<label>Community (0-255)</label>";
  html += "<input type='number' id='community' placeholder='1' min='0' max='255' value='" + String(local.community) + "'>";
  html += "</div>";
  html += "</div>";
  html += "<div class='grid'>";
  html += "<div>";
  html += "<label>Node (0-255)</label>";
  html += "<input type='number' id='node' placeholder='1' min='0' max='255' value='" + String(local.node) + "'>";
  html += "</div>";
  html += "<div class='button-group'>";
  html += "<button class='button' onclick='setAddress()'>SET ADDRESS</button>";
  html += "<button class='button' onclick='getStatus()'>GET STATUS</button>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  // User Guide Section
  html += "<div class='accordion'>";
  html += "<div class='accordion-header' onclick='toggleAccordion(\"guide\")'>";
  html += "<span>USER GUIDE</span>";
  html += "<svg class='accordion-icon' id='guide-icon' viewBox='0 0 24 24'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z'/></svg>";
  html += "</div>";
  html += "<div class='accordion-content' id='guide-content'>";
  html += "<h3>RadioDoge Features Overview</h3>";
  html += "<p>RadioDoge enables wireless P2P Dogecoin transactions and messaging via LoRa radio. No internet required!</p>";
  
  html += "<h3>Web Interface</h3>";
  html += "<p><strong>Access:</strong> Connect to WiFi 'RadioDoge' (password: radiodoge) and open <code>http://192.168.4.1</code></p>";
  html += "<p><strong>Features:</strong> Click any section header to expand and access controls for ping, messages, transactions, broadcasts, and device configuration.</p>";
  
  html += "<h3>Mobile Access</h3>";
  html += "<p><strong>WiFi Access:</strong> Connect to 'RadioDoge' WiFi network (password: radiodoge) and open <code>http://192.168.4.1</code></p>";
  html += "<p><strong>Note:</strong> Use WiFi web interface for mobile access.</p>";
  
  html += "<h3>API Commands (WiFi Only)</h3>";
  html += "<div style='background:#1a1a1a;padding:15px;border-radius:8px;margin:10px 0;font-family:monospace;'>";
  html += "<p><strong>Send Ping:</strong><br><code>GET /api/ping?region=10&community=1&node=2</code></p>";
  html += "<p><strong>Send Message:</strong><br><code>POST /api/message</code><br>Body: <code>address=10.1.2&type=text&message=Hello!</code></p>";
  html += "<p><strong>Send Dogecoin Transaction:</strong><br><code>POST /api/transaction</code><br>Body: <code>address=10.1.2&type=signed&data=0100000001...</code></p>";
  html += "<p><strong>Broadcast Message:</strong><br><code>POST /api/broadcast</code><br>Body: <code>type=announcement&priority=normal&message=Network update</code></p>";
  html += "<p><strong>Get Status:</strong><br><code>GET /api/status</code></p>";
  html += "<p><strong>Set Address:</strong><br><code>POST /api/address</code><br>Body: <code>region=10&community=1&node=5</code></p>";
  html += "</div>";
  
  html += "<h3>REST API</h3>";
  html += "<p><strong>Base URL:</strong> <code>http://192.168.4.1/api/</code></p>";
  html += "<p><strong>Format:</strong> JSON requests and responses</p>";
  html += "<p><strong>Methods:</strong> GET and POST supported</p>";
  
  html += "<h4>API Endpoints:</h4>";
  html += "<div style='background:#1a1a1a;padding:15px;border-radius:8px;margin:10px 0;font-family:monospace;'>";
  html += "<p><strong>Send Ping:</strong><br><code>GET /api/ping?region=10&community=1&node=2</code></p>";
  html += "<p><strong>Send Message:</strong><br><code>POST /api/message</code><br>Body: <code>address=10.1.2&type=text&text=Hello</code></p>";
  html += "<p><strong>Send Transaction:</strong><br><code>POST /api/transaction</code><br>Body: <code>address=10.1.2&type=signed&data=0100000001...</code></p>";
  html += "<p><strong>Broadcast:</strong><br><code>POST /api/broadcast</code><br>Body: <code>type=announcement&priority=normal&message=Update</code></p>";
  html += "<p><strong>Get Status:</strong><br><code>GET /api/status</code></p>";
  html += "<p><strong>Set Address:</strong><br><code>POST /api/address</code><br>Body: <code>region=10&community=1&node=5</code></p>";
  html += "</div>";
  
  html += "<h3>Dogecoin Transactions</h3>";
  html += "<p><strong>What you need:</strong> A <strong>signed Dogecoin transaction</strong> from your wallet</p>";
  html += "<p><strong>Transaction Types:</strong></p>";
  html += "<ul style='margin:10px 0;padding-left:20px;'>";
  html += "<li><strong>signed (Recommended):</strong> Complete signed transaction ready for broadcast - <em>This is what you should use</em></li>";
  html += "<li><strong>raw:</strong> Raw transaction data (advanced users)</li>";
  html += "<li><strong>utxo:</strong> UTXO data for transaction construction (advanced users)</li>";
  html += "</ul>";
  
  html += "<h4>How to Send Dogecoin via RadioDoge:</h4>";
  html += "<ol style='margin:10px 0;padding-left:20px;'>";
  html += "<li><strong>Create Transaction:</strong> Use your Dogecoin wallet to create a transaction</li>";
  html += "<li><strong>Sign Transaction:</strong> Sign the transaction with your private key (this creates a <strong>signed transaction</strong>)</li>";
  html += "<li><strong>Export Signed Transaction:</strong> Get the signed transaction as a hex string from your wallet</li>";
  html += "<li><strong>Send via RadioDoge:</strong> Use web interface or API to send the <strong>signed transaction</strong></li>";
  html += "<li><strong>Broadcast:</strong> RadioDoge transmits the signed transaction via LoRa to other devices</li>";
  html += "<li><strong>Network Propagation:</strong> Other RadioDoge devices can relay the signed transaction to the Dogecoin network</li>";
  html += "</ol>";
  
  html += "<h3>LoRa Network</h3>";
  html += "<p><strong>Address Format:</strong> Region.Community.Node (e.g., 10.1.3)</p>";
  html += "<p><strong>Range:</strong> Up to 10km in open areas</p>";
  html += "<p><strong>Frequency:</strong> 915 MHz (region dependent)</p>";
  html += "<p><strong>Power:</strong> Low power, battery friendly</p>";
  
  html += "<h3>Device Configuration</h3>";
  html += "<p><strong>Set Address:</strong> Use Device Configuration section to set your RadioDoge address</p>";
  html += "<p><strong>Address Range:</strong> Each component (Region, Community, Node) can be 0-255</p>";
  html += "<p><strong>Examples:</strong> 10.1.1, 10.1.2, 10.2.1, 255.255.255</p>";
  
  html += "<h3>Mobile Integration Examples</h3>";
  html += "<h4>Android (using curl):</h4>";
  html += "<div style='background:#1a1a1a;padding:15px;border-radius:8px;margin:10px 0;font-family:monospace;font-size:0.9em;'>";
  html += "<p># Send a message<br>curl -X POST \"http://192.168.4.1/api/message\" \\<br>&nbsp;&nbsp;-d \"address=10.1.2&type=text&text=Hello from Android!\"</p>";
  html += "<p># Send Dogecoin transaction<br>curl -X POST \"http://192.168.4.1/api/transaction\" \\<br>&nbsp;&nbsp;-d \"address=10.1.2&type=signed&data=0100000001...\"</p>";
  html += "</div>";
  
  html += "<h4>JavaScript (using fetch):</h4>";
  html += "<div style='background:#1a1a1a;padding:15px;border-radius:8px;margin:10px 0;font-family:monospace;font-size:0.9em;'>";
  html += "<p>// Send broadcast message<br>fetch('http://192.168.4.1/api/broadcast', {<br>&nbsp;&nbsp;method: 'POST',<br>&nbsp;&nbsp;headers: {<br>&nbsp;&nbsp;&nbsp;&nbsp;'Content-Type': 'application/x-www-form-urlencoded',<br>&nbsp;&nbsp;},<br>&nbsp;&nbsp;body: 'type=announcement&priority=normal&message=Hello from JavaScript!'<br>})<br>.then(response => response.json())<br>.then(data => console.log(data));</p>";
  html += "<br><p>// Send Dogecoin transaction<br>fetch('http://192.168.4.1/api/transaction', {<br>&nbsp;&nbsp;method: 'POST',<br>&nbsp;&nbsp;headers: {<br>&nbsp;&nbsp;&nbsp;&nbsp;'Content-Type': 'application/x-www-form-urlencoded',<br>&nbsp;&nbsp;},<br>&nbsp;&nbsp;body: 'address=10.1.2&type=signed&data=0100000001...'<br>})<br>.then(response => response.json())<br>.then(data => console.log(data));</p>";
  html += "</div>";
  
  html += "<h3>Important Notes</h3>";
  html += "<ul style='margin:10px 0;padding-left:20px;'>";
  html += "<li><strong>No Internet Required:</strong> RadioDoge works completely offline</li>";
  html += "<li><strong>P2P Network:</strong> Direct device-to-device communication</li>";
  html += "<li><strong>Range Limited:</strong> Devices must be within LoRa range</li>";
  html += "<li><strong>Transaction Security:</strong> Always verify transaction details before sending</li>";
  html += "<li><strong>Network Relay:</strong> Other RadioDoge devices can relay your transactions</li>";
  html += "<li><strong>Battery Life:</strong> LoRa is very power efficient</li>";
  html += "</ul>";
  
  html += "<h3>Troubleshooting</h3>";
  html += "<p><strong>Can't connect to WiFi:</strong> Make sure you're connecting to 'RadioDoge' network</p>";
  html += "<p><strong>API errors:</strong> Check that you're using the correct endpoint and parameters</p>";
  html += "<p><strong>No LoRa response:</strong> Verify target device is in range and powered on</p>";
  html += "<p><strong>Transaction failed:</strong> Ensure transaction is properly signed and formatted</p>";
  html += "</div>";
  html += "</div>";
  
  // Response Area
  html += "<div id='response' class='response' style='display:none;'></div>";
  
  // JavaScript
  html += "<script>";
  html += "function toggleAccordion(section){var content=document.getElementById(section+'-content');var icon=document.getElementById(section+'-icon');var header=icon.parentElement;if(content.classList.contains('active')){content.classList.remove('active');header.classList.remove('active');icon.classList.remove('rotated');}else{content.classList.add('active');header.classList.add('active');icon.classList.add('rotated');}}";
  html += "function showResponse(msg){document.getElementById('response').style.display='block';document.getElementById('response').innerHTML=msg;}";
  html += "function sendPing(){var r=document.getElementById('pingRegion').value;var c=document.getElementById('pingCommunity').value;var n=document.getElementById('pingNode').value;fetch('/ping?region='+r+'&community='+c+'&node='+n).then(r=>r.text()).then(d=>showResponse('PING: '+d));}";
  html += "function pingAll(){fetch('/ping?broadcast=1').then(r=>r.text()).then(d=>showResponse('BROADCAST PING: '+d));}";
  html += "function sendMessage(){var addr=document.getElementById('msgAddress').value;var type=document.getElementById('msgType').value;var msg=document.getElementById('message').value;fetch('/message?address='+encodeURIComponent(addr)+'&type='+type+'&text='+encodeURIComponent(msg)).then(r=>r.text()).then(d=>showResponse('MESSAGE: '+d));}";
  html += "function sendTransaction(){var addr=document.getElementById('txAddress').value;var type=document.getElementById('txType').value;var tx=document.getElementById('transaction').value;fetch('/transaction?address='+encodeURIComponent(addr)+'&type='+type+'&data='+encodeURIComponent(tx)).then(r=>r.text()).then(d=>showResponse('TRANSACTION: '+d));}";
  html += "function sendBroadcast(){var type=document.getElementById('broadcastType').value;var priority=document.getElementById('broadcastPriority').value;var msg=document.getElementById('broadcastMessage').value;fetch('/broadcast?type='+type+'&priority='+priority+'&message='+encodeURIComponent(msg)).then(r=>r.text()).then(d=>showResponse('BROADCAST: '+d));}";
  html += "function sendACK(){var addr=document.getElementById('ackAddress').value;var type=document.getElementById('ackType').value;fetch('/ack?address='+encodeURIComponent(addr)+'&type='+type).then(r=>r.text()).then(d=>showResponse('ACK: '+d));}";
  html += "function setAddress(){var r=document.getElementById('region').value;var c=document.getElementById('community').value;var n=document.getElementById('node').value;fetch('/address?region='+r+'&community='+c+'&node='+n).then(r=>r.text()).then(d=>{showResponse('ADDRESS: '+d);updateStatusDisplay(r+'.'+c+'.'+n);});}";
  html += "function updateStatusDisplay(newAddress){var addressSpan=document.querySelector('.address-display');if(addressSpan){addressSpan.textContent=newAddress;}}";
  html += "function getStatus(){fetch('/status').then(r=>r.text()).then(d=>showResponse('STATUS: '+d));}";
  html += "</script></div></body></html>";
  
  server.send(200, "text/html", html);
}

void handlePing() {
  if (server.hasArg("broadcast") && server.arg("broadcast") == "1") {
    // Broadcast ping to all devices
    SetDestinationAsBroadcast();
    SendPing(dest);
    server.send(200, "text/plain", "Broadcast ping sent to all devices");
  } else {
    // Send ping to specific address
    int region = server.hasArg("region") ? server.arg("region").toInt() : 10;
    int community = server.hasArg("community") ? server.arg("community").toInt() : 1;
    int node = server.hasArg("node") ? server.arg("node").toInt() : 2;
    
    nodeAddress pingDest = {region, community, node};
    SendPing(pingDest);
    server.send(200, "text/plain", "Ping sent to " + String(region) + "." + String(community) + "." + String(node));
  }
}

void handleMessage() {
  if (server.hasArg("text")) {
    String message = server.arg("text");
    String address = server.hasArg("address") ? server.arg("address") : "10.1.2";
    String type = server.hasArg("type") ? server.arg("type") : "text";
    
    // Parse address
    int firstDot = address.indexOf('.');
    int secondDot = address.indexOf('.', firstDot + 1);
    int region = address.substring(0, firstDot).toInt();
    int community = address.substring(firstDot + 1, secondDot).toInt();
    int node = address.substring(secondDot + 1).toInt();
    
    nodeAddress msgDest = {region, community, node};
    dest = msgDest;
    
    // Convert message to bytes
    int messageLength = message.length();
    if (messageLength > 200) messageLength = 200;  // Limit message size
    
    // Prepare message packet
    uint8_t messagePacket[256];
    messagePacket[0] = (uint8_t)MESSAGE;
    messagePacket[1] = 6;  // Header size
    messagePacket[2] = local.region;
    messagePacket[3] = local.community;
    messagePacket[4] = local.node;
    messagePacket[5] = dest.region;
    messagePacket[6] = dest.community;
    messagePacket[7] = dest.node;
    
    // Add message content
    for (int i = 0; i < messageLength; i++) {
      messagePacket[8 + i] = message[i];
    }
    
    // Send the message
    Radio.Send(messagePacket, 8 + messageLength);
    DisplayTXMessage(message, dest);
    
    server.send(200, "text/plain", "Message sent to " + address + ": " + message);
  } else {
    server.send(400, "text/plain", "No message provided");
  }
}

void handleAddress() {
  if (server.hasArg("region") && server.hasArg("community") && server.hasArg("node")) {
    // Set new address
    local.region = server.arg("region").toInt();
    local.community = server.arg("community").toInt();
    local.node = server.arg("node").toInt();
    InitControlMessages();
    DisplayLocalAddress(local);
    server.send(200, "text/plain", "Address set to " + String(local.region) + "." + String(local.community) + "." + String(local.node));
  } else {
    // Get current address
    server.send(200, "text/plain", String(local.region) + "." + String(local.community) + "." + String(local.node));
  }
}

void handleTransaction() {
  if (server.hasArg("data")) {
    String transaction = server.arg("data");
    String address = server.hasArg("address") ? server.arg("address") : "10.1.2";
    String type = server.hasArg("type") ? server.arg("type") : "signed";
    
    // Parse address
    int firstDot = address.indexOf('.');
    int secondDot = address.indexOf('.', firstDot + 1);
    int region = address.substring(0, firstDot).toInt();
    int community = address.substring(firstDot + 1, secondDot).toInt();
    int node = address.substring(secondDot + 1).toInt();
    
    nodeAddress txDest = {region, community, node};
    dest = txDest;
    
    // Convert transaction to bytes
    int txLength = transaction.length();
    if (txLength > 200) txLength = 200;  // Limit transaction size
    
    // Prepare transaction packet
    uint8_t txPacket[256];
    txPacket[0] = (uint8_t)HOST_FORMED_PACKET;  // Use custom packet type for transactions
    txPacket[1] = 6;  // Header size
    txPacket[2] = local.region;
    txPacket[3] = local.community;
    txPacket[4] = local.node;
    txPacket[5] = dest.region;
    txPacket[6] = dest.community;
    txPacket[7] = dest.node;
    
    // Add transaction content
    for (int i = 0; i < txLength; i++) {
      txPacket[8 + i] = transaction[i];
    }
    
    // Send the transaction
    Radio.Send(txPacket, 8 + txLength);
    DisplayTXMessage("Transaction: " + transaction.substring(0, 20) + "...", dest);
    
    server.send(200, "text/plain", "Transaction sent to " + address + " (" + type + ")");
  } else {
    server.send(400, "text/plain", "No transaction data provided");
  }
}

void handleBroadcast() {
  if (server.hasArg("message")) {
    String message = server.arg("message");
    String type = server.hasArg("type") ? server.arg("type") : "announcement";
    String priority = server.hasArg("priority") ? server.arg("priority") : "normal";
    
    // Set destination as broadcast
    SetDestinationAsBroadcast();
    
    // Convert message to bytes
    int messageLength = message.length();
    if (messageLength > 200) messageLength = 200;  // Limit message size
    
    // Prepare broadcast packet
    uint8_t broadcastPacket[256];
    broadcastPacket[0] = (uint8_t)MESSAGE;  // Use MESSAGE type for broadcast
    broadcastPacket[1] = 6;  // Header size
    broadcastPacket[2] = local.region;
    broadcastPacket[3] = local.community;
    broadcastPacket[4] = local.node;
    broadcastPacket[5] = 255;  // Broadcast address
    broadcastPacket[6] = 255;  // Broadcast address
    broadcastPacket[7] = 255;  // Broadcast address
    
    // Add message content
    for (int i = 0; i < messageLength; i++) {
      broadcastPacket[8 + i] = message[i];
    }
    
    // Send the broadcast
    Radio.Send(broadcastPacket, 8 + messageLength);
    DisplayBroadcastMessage("Broadcast: " + message, local);
    
    server.send(200, "text/plain", "Broadcast sent (" + type + ", " + priority + "): " + message);
  } else {
    server.send(400, "text/plain", "No broadcast message provided");
  }
}

void handleACK() {
  if (server.hasArg("address")) {
    String address = server.arg("address");
    String type = server.hasArg("type") ? server.arg("type") : "received";
    
    // Parse address
    int firstDot = address.indexOf('.');
    int secondDot = address.indexOf('.', firstDot + 1);
    int region = address.substring(0, firstDot).toInt();
    int community = address.substring(firstDot + 1, secondDot).toInt();
    int node = address.substring(secondDot + 1).toInt();
    
    nodeAddress ackDest = {region, community, node};
    SendACK(ackDest);
    
    server.send(200, "text/plain", "ACK sent to " + address + " (" + type + ")");
  } else {
    server.send(400, "text/plain", "No ACK target address provided");
  }
}

void handleStatus() {
  String status = "RadioDoge - Ready\n";
  status += "Address: " + String(local.region) + "." + String(local.community) + "." + String(local.node) + "\n";
  status += "LoRa: Active\n";
  status += "Display: Working\n";
  status += "WiFi: " + String(ssid) + "\n";
  status += "Uptime: " + String(millis() / 1000) + " seconds\n";
  status += "Free Memory: " + String(ESP.getFreeHeap()) + " bytes";
  server.send(200, "text/plain", status);
}


// JSON API Handlers
void handleApiPing() {
  String response = "{";
  response += "\"success\":true,";
  response += "\"timestamp\":" + String(millis()) + ",";
  
  if (server.hasArg("broadcast") && server.arg("broadcast") == "1") {
    SetDestinationAsBroadcast();
    SendPing(dest);
    response += "\"action\":\"broadcast_ping\",";
    response += "\"message\":\"Broadcast ping sent to all devices\"";
  } else {
    int region = server.hasArg("region") ? server.arg("region").toInt() : 10;
    int community = server.hasArg("community") ? server.arg("community").toInt() : 1;
    int node = server.hasArg("node") ? server.arg("node").toInt() : 2;
    
    nodeAddress pingDest = {region, community, node};
    SendPing(pingDest);
    response += "\"action\":\"ping\",";
    response += "\"target\":\"" + String(region) + "." + String(community) + "." + String(node) + "\",";
    response += "\"message\":\"Ping sent to " + String(region) + "." + String(community) + "." + String(node) + "\"";
  }
  
  response += "}";
  server.send(200, "application/json", response);
}

void handleApiMessage() {
  String response = "{";
  response += "\"success\":true,";
  response += "\"timestamp\":" + String(millis()) + ",";
  
  if (server.hasArg("text")) {
    String message = server.arg("text");
    String address = server.hasArg("address") ? server.arg("address") : "10.1.2";
    String type = server.hasArg("type") ? server.arg("type") : "text";
    
    // Parse address
    int dots[2];
    int dotCount = 0;
    for (int i = 0; i < address.length() && dotCount < 2; i++) {
      if (address.charAt(i) == '.') {
        dots[dotCount] = i;
        dotCount++;
      }
    }
    
    if (dotCount == 2) {
      int region = address.substring(0, dots[0]).toInt();
      int community = address.substring(dots[0] + 1, dots[1]).toInt();
      int node = address.substring(dots[1] + 1).toInt();
      
      nodeAddress msgDest = {region, community, node};
      SendMessage(msgDest, message, type);
      
      response += "\"action\":\"message\",";
      response += "\"target\":\"" + address + "\",";
      response += "\"type\":\"" + type + "\",";
      response += "\"message\":\"Message sent to " + address + "\"";
    } else {
      response += "\"success\":false,";
      response += "\"error\":\"Invalid address format. Use REGION.COMMUNITY.NODE\"";
    }
  } else {
    response += "\"success\":false,";
    response += "\"error\":\"No message text provided\"";
  }
  
  response += "}";
  server.send(200, "application/json", response);
}

void handleApiTransaction() {
  String response = "{";
  response += "\"success\":true,";
  response += "\"timestamp\":" + String(millis()) + ",";
  
  if (server.hasArg("data")) {
    String transaction = server.arg("data");
    String address = server.hasArg("address") ? server.arg("address") : "10.1.2";
    String type = server.hasArg("type") ? server.arg("type") : "signed";
    
    // Parse address
    int dots[2];
    int dotCount = 0;
    for (int i = 0; i < address.length() && dotCount < 2; i++) {
      if (address.charAt(i) == '.') {
        dots[dotCount] = i;
        dotCount++;
      }
    }
    
    if (dotCount == 2) {
      int region = address.substring(0, dots[0]).toInt();
      int community = address.substring(dots[0] + 1, dots[1]).toInt();
      int node = address.substring(dots[1] + 1).toInt();
      
      nodeAddress txDest = {region, community, node};
      SendTransaction(txDest, transaction, type);
      
      response += "\"action\":\"transaction\",";
      response += "\"target\":\"" + address + "\",";
      response += "\"type\":\"" + type + "\",";
      response += "\"message\":\"Transaction sent to " + address + "\"";
    } else {
      response += "\"success\":false,";
      response += "\"error\":\"Invalid address format. Use REGION.COMMUNITY.NODE\"";
    }
  } else {
    response += "\"success\":false,";
    response += "\"error\":\"No transaction data provided\"";
  }
  
  response += "}";
  server.send(200, "application/json", response);
}

void handleApiBroadcast() {
  String response = "{";
  response += "\"success\":true,";
  response += "\"timestamp\":" + String(millis()) + ",";
  
  if (server.hasArg("message")) {
    String message = server.arg("message");
    String type = server.hasArg("type") ? server.arg("type") : "announcement";
    String priority = server.hasArg("priority") ? server.arg("priority") : "normal";
    
    SendBroadcast(message, type, priority);
    
    response += "\"action\":\"broadcast\",";
    response += "\"type\":\"" + type + "\",";
    response += "\"priority\":\"" + priority + "\",";
    response += "\"message\":\"Broadcast sent to all devices\"";
  } else {
    response += "\"success\":false,";
    response += "\"error\":\"No broadcast message provided\"";
  }
  
  response += "}";
  server.send(200, "application/json", response);
}

void handleApiStatus() {
  String response = "{";
  response += "\"success\":true,";
  response += "\"timestamp\":" + String(millis()) + ",";
  response += "\"device\":{";
  response += "\"name\":\"RadioDoge\",";
  response += "\"address\":\"" + String(local.region) + "." + String(local.community) + "." + String(local.node) + "\",";
  response += "\"status\":\"Ready\",";
  response += "\"lora\":\"Active\",";
  response += "\"wifi\":\"" + String(ssid) + "\",";
  response += "\"uptime\":" + String(millis() / 1000) + ",";
  response += "\"free_memory\":" + String(ESP.getFreeHeap());
  response += "}";
  response += "}";
  server.send(200, "application/json", response);
}

void handleApiAddress() {
  String response = "{";
  response += "\"success\":true,";
  response += "\"timestamp\":" + String(millis()) + ",";
  
  if (server.method() == HTTP_POST) {
    // Set new address
    if (server.hasArg("region") && server.hasArg("community") && server.hasArg("node")) {
      local.region = server.arg("region").toInt();
      local.community = server.arg("community").toInt();
      local.node = server.arg("node").toInt();
      InitControlMessages();
      DisplayLocalAddress(local);
      
      response += "\"action\":\"set_address\",";
      response += "\"address\":\"" + String(local.region) + "." + String(local.community) + "." + String(local.node) + "\",";
      response += "\"message\":\"Address updated successfully\"";
    } else {
      response += "\"success\":false,";
      response += "\"error\":\"Missing required parameters: region, community, node\"";
    }
  } else {
    // Get current address
    response += "\"action\":\"get_address\",";
    response += "\"address\":\"" + String(local.region) + "." + String(local.community) + "." + String(local.node) + "\"";
  }
  
  response += "}";
  server.send(200, "application/json", response);
}