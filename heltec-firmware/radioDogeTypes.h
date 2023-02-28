#ifndef RADIO_DOGE_TYPES
#define RADIO_DOGE_TYPES

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

#endif