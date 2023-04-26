#ifndef RADIO_DOGE_TYPES
#define RADIO_DOGE_TYPES

enum messageType {
  UNKNOWN,
  ACK,
  PING,
  MESSAGE
};

enum displayType{
  CUSTOM_DISPLAY,
  LOGO_DISPLAY,
};

enum serialCommand {
  NONE,
  ADDRESS_GET,
  ADDRESS_SET,
  PING_REQUEST,
  MESSAGE_REQUEST,
  HARDWARE_INFO = 63,        //0x3f = '?'
  DISPLAY_CONTROL = 100, // 0x64 = 'd'
  HOST_FORMED_PACKET = 104,  //0x68 = 'h'
  MULTIPART_PACKET = 109, // 0x6D = 'm'
  RESULT_CODE = 254
};

struct nodeAddress {
  uint8_t region;
  uint8_t community;
  uint8_t node;
};

#endif