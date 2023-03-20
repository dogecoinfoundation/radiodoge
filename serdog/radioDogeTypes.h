#ifndef RADIO_DOGE_TYPES
#define RADIO_DOGE_TYPES

#pragma once
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
	RESULT_CODE=254 //0xFE
};

enum resultCode {
	RESULT_ACK=6, // 0x06 = 'ACK'
	RESULT_NACK=21 // 0x15 = 'NAK'
};

struct nodeAddress {
	uint8_t region;
	uint8_t community;
	uint8_t node;
};

#endif