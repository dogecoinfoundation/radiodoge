#ifndef RADIO_DOGE_TYPES
#define RADIO_DOGE_TYPES

#pragma once
enum serialCommand 
{
	NONE,
	NODE_ADDRESS_GET,
	NODE_ADDRESS_SET,
	PING_REQUEST,
	MESSAGE_REQUEST,
	HARDWARE_INFO = 63, //0x3f = '?'
	DISPLAY_CONTROL = 100, // 0x64 = 'd'
	HOST_FORMED_PACKET = 104, //0x68 = 'h'
	MULTIPART_PACKET = 109, // 0x6D = 'm'
	RESULT_CODE=254 //0xFE
};

enum displayType
{
	CUSTOM_DISPLAY,
	RADIO_DOGE_LOGO,
	DOGE_ANIMATION,
	COIN_ANIMATION
};

enum resultCode 
{
	RESULT_ACK=6, // 0x06 = 'ACK'
	RESULT_NACK=21 // 0x15 = 'NAK'
};

enum dogeCommand
{
	DISPLAY_QR = 1,
	REGISTER_ADDRESS = 15,
	GET_DOGE_ADDRESS = 120,
	SEND_DOGE_ADDRESS = 240,
	GET_BALANCE = 101,
	SEND_BALANCE = 202
};

#define PIN_LENGTH 4

struct nodeAddress {
	uint8_t region;
	uint8_t community;
	uint8_t node;
};

#endif