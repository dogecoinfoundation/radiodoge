// Prototype RadioDoge firmware for the Heltec WiFi LoRa 32 v2 & v3 modules
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "logoImage.h"
#include "coin.h"
#include "doge.h"
#include "sendingDogeCoin.h"
#include "receivingDogeCoin.h"

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
#define SERIAL_HEADER_SIZE 8
#define BUFFER_SIZE 256  // Define the payload size here
#define CONTROL_SIZE 8   // Really only 7 but we will have a reserved byte here for now
#define ANIMATION_FRAME_DELAY 5
#define MIDDLE_OF_SCREEN 27  // Assumes text is 10 pixels in length so half would be (64/2) - (10/2) = 27
#define SERIAL_TERMINATOR 255
#define TEST_COIN_AMOUNT 420.69

#ifdef WIFI_LoRa_32_V2
#define HELTEC_BOARD_VERSION 2
#else
#define HELTEC_BOARD_VERSION 3
#endif

char displayBuf[64];
char txPacket[BUFFER_SIZE];
char rxPacket[BUFFER_SIZE];
uint8_t controlPacket[CONTROL_SIZE];
uint8_t serialBuf[BUFFER_SIZE];
uint8_t serialHeader[SERIAL_HEADER_SIZE];

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t messageSize, int16_t rssiMeasured, int8_t snr);
SSD1306Wire oledDisplay(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

int16_t rssi;
int16_t rxSize;
bool isLoRaIdle = true;
bool needToSendACK = false;
int xPos = 0;

enum messageType {
  UNKNOWN,
  ACK,
  PING,
  MESSAGE
};

enum serialCommand {
  NONE,
  ADDRESS_GET,
  ADDRESS_SET,
  PING_REQUEST,
  MESSAGE_REQUEST,
  HARDWARE_INFO = 63,        //0x3f = '?'
  HOST_FORMED_PACKET = 104,  //0x68 = 'h'
};

struct nodeAddress {
  uint8_t region;
  uint8_t community;
  uint8_t node;
};

nodeAddress local;
nodeAddress dest;

void setup() {
  Serial.begin(115200);
  Mcu.begin();
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
  // Init the display
  oledDisplay.init();
  oledDisplay.setTextAlignment(TEXT_ALIGN_LEFT);
  oledDisplay.setFont(ArialMT_Plain_10);
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
  // Leaving setup (debug)
  Serial.println("VERY SETUP");
}

void loop() {
  //RawSerialMessageSendAndReceive()
  CommandAndControlLoop();
}

// Older test that allows for the sending of messages (specified from the host over serial) between devices without any addressing
void RawSerialMessageSendAndReceive() {
  if (isLoRaIdle) {
    isLoRaIdle = false;
    // Indicating that we are moving into rx mode (debug)
    Serial.println("VERY RX");
    // Enter back into RX mode
    Radio.Rx(0);
  }

  // Check if there is data on the serial port to send out
  if (CreateMessage()) {
    isLoRaIdle = false;
    // Indicating that we are starting a TX
    Serial.println("VERY TX");
    Serial.printf("\r\nSending packet \"%X,\" , Length %d\r\n", txPacket, strlen(txPacket));
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
    Serial.println("VERY RX");
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
  Serial.println("WOW MUCH TX");
  isLoRaIdle = true;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  // Indicate that TX failed (debug)
  Serial.println("OOPS TX BAD");
  isLoRaIdle = true;
}

void OnRxTimeout(void) {
  Radio.Sleep();
  // Indicate that RX failed (debug)
  // Probably should not see this since RX should not timeout
  Serial.println("OOPS RX BAD");
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
  Serial.println("WOW MUCH RX");
  Serial.printf("\r\nReceived packet! Rssi %d , Length %d\r\n", rssi, rxSize);
  ParseReceivedMessage();
  isLoRaIdle = true;
}

// Create a custom message that is specified by the host over the serial port
bool CreateMessage() {
  if (Serial.available() > 0) {
    String readString = Serial.readStringUntil('\0');
    // Echo back the read string (debug)
    Serial.printf("REPLY: %s\n", readString);
    // Display on the screen
    DisplayTXMessage(readString);
    readString.toCharArray(txPacket, BUFFER_SIZE);
    return true;
  }
  return false;
}

// Display a transmitted message on the module's screen
void DisplayTXMessage(String toDisplay) {
  oledDisplay.clear();
  sprintf(displayBuf, "Sending to %d.%d.%d:", dest.region, dest.community, dest.node);
  oledDisplay.drawString(0, 5, displayBuf);
  oledDisplay.drawString(0, MIDDLE_OF_SCREEN, toDisplay);
  oledDisplay.display();
}

// Display a received message on the module's screen
void DisplayRXMessage(String rxMessage) {
  oledDisplay.clear();
  sprintf(displayBuf, "Received from %d.%d.%d:", rxPacket[1], rxPacket[2], rxPacket[3]);
  oledDisplay.drawString(0, 5, displayBuf);
  oledDisplay.drawString(0, MIDDLE_OF_SCREEN, rxMessage);
  oledDisplay.display();
}

// Display received command information from the host on the module's screen
void DisplayCommandAndControl(uint8_t commandVal) {
  oledDisplay.clear();
  sprintf(displayBuf, "CMD FR HOST: %d", commandVal);
  oledDisplay.drawString(0, 5, displayBuf);
  String commandTypeString;
  switch (commandVal) {
    case ADDRESS_GET:
      commandTypeString = "Get Address";
      break;
    case ADDRESS_SET:
      commandTypeString = "Set Address";
      break;
    case PING_REQUEST:
      commandTypeString = "Send Ping";
      break;
    case MESSAGE_REQUEST:
      commandTypeString = "Send Message";
      break;
    case HARDWARE_INFO:
      commandTypeString = "Get Hardware Info";
      break;
    case HOST_FORMED_PACKET:
      commandTypeString = "Host formed packet";
      break;
    default:
      commandTypeString = "Wat!?";
      break;
  }
  oledDisplay.drawString(0, MIDDLE_OF_SCREEN, commandTypeString);
  oledDisplay.display();
}

// Display hardware information on the screen
void DisplayHardwareInfo() {
  oledDisplay.clear();
  sprintf(displayBuf, "Heltec WiFi LoRa 32 V%d", HELTEC_BOARD_VERSION);
  oledDisplay.drawString(0, MIDDLE_OF_SCREEN, displayBuf);
  oledDisplay.display();
}

// Display the modules local address on the screen
void DisplayLocalAddress() {
  oledDisplay.clear();
  sprintf(displayBuf, "Hi I'm %d.%d.%d", local.region, local.community, local.node);
  oledDisplay.drawString(0, MIDDLE_OF_SCREEN, displayBuf);
  oledDisplay.display();
}

// Draw static RadioDoge Logo
void DrawRadioDogeLogo() {
  oledDisplay.clear();
  oledDisplay.drawXbm(0, 0, logo_width, logo_height, logo_bits);
  oledDisplay.display();
}

// Draw sending coin image
void DrawSendingCoinsImage(float numCoins) {
  oledDisplay.clear();
  oledDisplay.drawXbm(0, 0, sendCoin_width, sendCoin_height, sendCoin_bits);
  sprintf(displayBuf, "%0.2f", numCoins);
  oledDisplay.drawString(5, MIDDLE_OF_SCREEN, displayBuf);
  oledDisplay.display();
}

// Draw receiving coins image
void DrawReceivingCoinsImage(float numCoins) {
  oledDisplay.clear();
  oledDisplay.drawXbm(0, 0, rcvCoin_width, rcvCoin_height, rcvCoin_bits);
  sprintf(displayBuf, "%0.2f", numCoins);
  oledDisplay.drawString(5, MIDDLE_OF_SCREEN, displayBuf);
  oledDisplay.display();
}

// Animates a coin icon going across the screen
void CoinAnimation() {
  xPos = 0;
  while (true) {
    oledDisplay.clear();
    oledDisplay.drawXbm(xPos - 64, 0, coin_width, coin_height, coin_bits);
    oledDisplay.display();
    xPos++;
    if (xPos % 192 == 0) {
      oledDisplay.clear();
      return;
    }
    delay(ANIMATION_FRAME_DELAY);
  }
}

// Animates a doge face going across the screen
void DogeAnimation() {
  xPos = 0;
  while (true) {
    oledDisplay.clear();
    oledDisplay.drawXbm(xPos - 64, 0, doge_width, doge_height, doge_bits);
    oledDisplay.display();
    xPos++;
    if (xPos % 192 == 0) {
      oledDisplay.clear();
      return;
    }
    delay(ANIMATION_FRAME_DELAY);
  }
}

// Prepopulates control message buffers with local address
void InitControlMessages() {
  controlPacket[0] = (uint8_t)UNKNOWN;
  controlPacket[1] = local.region;
  controlPacket[2] = local.community;
  controlPacket[3] = local.node;
  controlPacket[7] = 0x00;
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

// Retrieve the local address
void GetLocalAddress() {
  Serial.printf("MUCH LCL: %d.%d.%d\n", local.region, local.community, local.node);
}

// Indicate to the host that an ACK was received and display it on the screen
void ReceivedACK() {
  Serial.printf("MUCH ACK FR %d.%d.%d\n", rxPacket[1], rxPacket[2], rxPacket[3]);
  DisplayRXMessage("ACK");
}

// Indicate to the host that a ping was received and display it on the screen
void ReceivedPing() {
  Serial.printf("MUCH PING FR %d.%d.%d\n", rxPacket[1], rxPacket[2], rxPacket[3]);
  DisplayRXMessage("Ping!");
}

// Send a ping to the specified destination address
void SendPing(nodeAddress destination) {
  // Message structure will by [message type, sender, destination]
  controlPacket[0] = (uint8_t)PING;
  controlPacket[4] = destination.region;
  controlPacket[5] = destination.community;
  controlPacket[6] = destination.node;
  isLoRaIdle = false;
  DisplayTXMessage("Ping");
  Serial.printf("Sending Ping to %d.%d.%d\n", destination.region, destination.community, destination.node);
  Radio.Send(controlPacket, CONTROL_SIZE);
}

// Send an ACK to the specified destination address
void SendACK(nodeAddress destination) {
  // Message structure will by [message type, sender, destination]
  controlPacket[0] = (uint8_t)ACK;
  controlPacket[4] = destination.region;
  controlPacket[5] = destination.community;
  controlPacket[6] = destination.node;
  DisplayTXMessage("ACK");
  isLoRaIdle = false;
  Serial.printf("Sending ACK to %d.%d.%d\n", destination.region, destination.community, destination.node);
  Radio.Send(controlPacket, CONTROL_SIZE);
}

// Basically we will just send out the the serial buffer
// Change up the first byte to indicate messageType=MESSAGE
void SendMessage(int messageLength) {
  // For now we will cap the size of the message we can send
  // Will have to investigate breaking up large messages in future but Radio.Send only accepts a uint8_t as the buffer size
  if (messageLength > 255) {
    messageLength = 255;
  }

  // Display message on display
  String messageString = ExtractStringMessageFromBuffer(serialBuf, messageLength);
  DisplayTXMessage(messageString);

  // Update packet header and send the message over the air
  serialBuf[0] = (uint8_t)MESSAGE;
  Radio.Send(serialBuf, (uint8_t)messageLength);
}

// Read the serial header and extract the command type and payload size from it.
// Currently the header consists of 8 bytes: the command type repeated 4 times and the payload size repeated 4 times.
bool ReadSerialHeader(serialCommand &commandType, uint8_t &payloadSize) {
  size_t numRead = Serial.readBytes(serialHeader, SERIAL_HEADER_SIZE);
  if (numRead != SERIAL_HEADER_SIZE) {
    return false;
  }
  // First 4 bytes will be command
  // Next 4 bytes will be payload size
  // For redundancy we will require command and payload repeated 4 times each
  bool successfulParse = (serialHeader[0] == serialHeader[1] && serialHeader[0] == serialHeader[2] && serialHeader[0] == serialHeader[3]);
  successfulParse &= (serialHeader[4] == serialHeader[5] && serialHeader[4] == serialHeader[6] && serialHeader[4] == serialHeader[7]);
  commandType = (serialCommand)serialHeader[0];
  payloadSize = serialHeader[4];
  return successfulParse;
}

// Read the host payload into the serial buffer.
bool ReadSerialPayload(uint8_t payloadSize) {
  uint8_t bytesRead = Serial.readBytes(serialBuf, payloadSize);
  if (bytesRead != payloadSize) {
    Serial.println("Serial read failure!");
    return false;
  }
  Serial.printf("Read %d bytes\n", bytesRead);
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
  Serial.printf("Command From Host Received: %d\n", commandVal);
  // Display the command on the module screen for now and wait for debug purposes
  DisplayCommandAndControl(commandVal);
  // Now we will read in the host's payload
  bool payloadSuccess = ReadSerialPayload(payloadSize);
  if (!payloadSuccess) {
    return;
  }
  delay(2000);
  switch (commandVal) {
    case ADDRESS_GET:
      GetLocalAddress();
      DisplayLocalAddress();
      break;
    case ADDRESS_SET:
      SetLocalAddressFromSerialBuffer(0);
      InitControlMessages();
      DisplayLocalAddress();
      break;
    case PING_REQUEST:
      SetDestinationFromSerialBuffer(0);
      SendPing(dest);
      break;
    case MESSAGE_REQUEST:
      SetDestinationFromSerialBuffer(4);
      SendMessage(payloadSize);
      break;
    case HARDWARE_INFO:
      DisplayHardwareInfo();
      Serial.printf("RD HT V%d FW01\n", HELTEC_BOARD_VERSION);
      break;
    case HOST_FORMED_PACKET:
      ParseHostFormedPacket(payloadSize);
      break;
    default:
      // Indicate that command was not understood (debug)
      Serial.println("wat cmd?");
      break;
  }
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

// Parse a serial command from the host and perform the desired function
// This assumes that the host sends '0xff' as a terminator
void ParseSerialReadWithTerminators() {
  int readLength = Serial.readBytesUntil(SERIAL_TERMINATOR, serialBuf, BUFFER_SIZE);
  Serial.printf("REPLY: Read %d bytes\n", readLength);
  if (readLength > 0) {
    // First byte read will be the command type
    serialCommand commType = (serialCommand)serialBuf[0];
    Serial.printf("Command From Host Received: %d\n", commType);
    // Display the command on the module screen for now and wait for debug purposes
    DisplayCommandAndControl(serialBuf[0]);
    delay(2000);
    switch (commType) {
      case ADDRESS_GET:
        GetLocalAddress();
        DisplayLocalAddress();
        break;
      case ADDRESS_SET:
        SetLocalAddressFromSerialBuffer(1);
        InitControlMessages();
        DisplayLocalAddress();
        break;
      case PING_REQUEST:
        SetDestinationFromSerialBuffer(1);
        SendPing(dest);
        break;
      case MESSAGE_REQUEST:
        SetDestinationFromSerialBuffer(1);
        SendMessage(readLength);
        break;
      case HARDWARE_INFO:
        DisplayHardwareInfo();
        Serial.printf("RD HT V%d FW01\n", HELTEC_BOARD_VERSION);
        break;
      default:
        // Indicate that command was not understood (debug)
        Serial.println("wat cmd?");
        break;
    }
  }
}

// Parse a LoRa message received over the air from another module
void ParseReceivedMessage() {
  if (CheckIfPacketForMe()) {
    Serial.println("PACKET FOR ME");
    messageType mType = (messageType)rxPacket[0];
    switch (mType) {
      case ACK:
        ReceivedACK();
        break;
      case PING:
        ReceivedPing();
        // Set the ACK destination as the address of the sender
        dest.region = rxPacket[1];
        dest.community = rxPacket[2];
        dest.node = rxPacket[3];
        // For some reason sending the ACK here works on V2 but not V3
        // Not sure why but sending it in the main loop works for both so we will do it there
        // We will just set a flag to send an ACK
        needToSendACK = true;
        break;
      case MESSAGE:
        {
          String messageString = ExtractStringMessageFromBuffer((uint8_t *)rxPacket, rxSize);
          DisplayRXMessage(messageString);
          Serial.printf("MUCH TLK FR %d.%d.%d:\n", rxPacket[1], rxPacket[2], rxPacket[3]);
          Serial.println(messageString);
        }
        break;
      default:
        // (debug)
        Serial.println("wat rcv?");
        break;
    }
  } else {
    Serial.printf("NOT FR ME: FR %d.%d.%d\n", rxPacket[4], rxPacket[5], rxPacket[6]);
  }
}

// Checks to see if the received packet's destination is the same as the local address
bool CheckIfPacketForMe() {
  return (local.region == rxPacket[4]) && (local.community == rxPacket[5]) && (local.node == rxPacket[6]);
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