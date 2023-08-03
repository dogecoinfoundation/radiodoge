// Prototype RadioDoge firmware for the Heltec WiFi LoRa 32 v2 & v3 modules
#include <Wire.h>
#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "radioDogeDisplay.h"
#include "radioDogeTypes.h"

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
  HeltecDisplayInit();
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
  //Serial.println("VERY SETUP");
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
      SendMessage(payloadSize);
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