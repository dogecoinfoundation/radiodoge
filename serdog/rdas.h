
#ifndef RDAS
#define RDAS

//RDAS
uint8_t rdas_attn[2] = {0x53, 0x4F}; //SO - "AT" equivalent (cmd attention)
uint8_t rdas_res[4] = {0x4D, 0x55, 0x43, 0x48}; //MUCH - Result code follows.
uint8_t rdas_ack[3] = {0x41, 0x43, 0x4B}; //ACK - acknowledge
uint8_t rdas_nak[3] = {0x4E, 0x41, 0x4B}; //NAK - remote error or nack
uint8_t rdas_err[3] = {0x57, 0x41, 0x54}; //WAT - local error
uint8_t rdas_from[2] = {0x46, 0x52}; // FR (from) Node Address follows[3]
uint8_t rdas_to[2] = {0x54, 0x4F}; // TO noun is Node Address follows[3]
uint8_t rdas_rxal[2] = {0x52, 0x58}; // RX - received packet alert
uint8_t rdas_this[4] = {0x4D, 0x49, 0x4E, 0x45};// MINE: (message alert is for me) MSGTYPE, FR, ADDR

//FOR ME will be removed
uint8_t rdas_forme[6] = { 0x46, 0x4F, 0x52, 0x20, 0x4D, 0x45 };// FOR ME Message follows: MSGTYPE, FR, ADDR

uint8_t rdas_msg[3] = {0x4D, 0x53, 0x47}; //MSG, noun follows is a clear msg
uint8_t rdas_quer[3] = {0x47, 0x49, 0x42}; //GIB - verb is a remote query
uint8_t rdas_ansr[3] = {0x57, 0x4F, 0x57}; //WOW - this is a response from a query
uint8_t rdas_txn[3] = {0x54, 0x58, 0x4E}; //TXN - noun is an outgoing transaction
uint8_t rdas_bal[3] = {0x42, 0x41, 0x4C}; //BAL - noun is balance (or returnval is balance)
uint8_t rdas_amtd[3] = {0x41, 0x4D, 0x54}; //AMT - amount in whole dogecoin
uint8_t rdas_koin[3] = {0x4B, 0x4E, 0x55}; //KNU - Mantissa amount in koinu
uint8_t rdas_dest[3] = {0x44, 0x53, 0x54}; //DST - Noun that follows is a destination (dogecoin address)
uint8_t rdas_src[3] = {0x53, 0x52, 0x43}; //SRC - Noun that follows is a source (utxo, index, address)

uint8_t rdas_pay[3] = {0x50, 0x41, 0x59}; //PAY - ask the node to pay AMOUNT/KOINU
uint8_t rdas_ask[3] = {0x41, 0x53 ,0x4B}; //ASK - request the node for a payment of AMOUNT/KOINU
uint8_t rdas_addr[3] = {0x41, 0x44, 0x52}; //ADR - noun that follows is a dogecoin address
uint8_t rdas_utxo[3] = {0x55, 0x54, 0x58}; //UTX - noun that follows is a UTXO
uint8_t rdas_indx[3] = {0x49, 0x44, 0x58}; //IDX - noun that follows is as utxo index.
uint8_t rdas_txid[3] = {0x54, 0x49, 0x44}; //TID - noun that follows is a transaction id.
uint8_t rdas_blk[3] = {0x42, 0x4C, 0x4B}; //BLK - noun that follows is a block/height.
uint8_t rdas_qr[3] = {0x51, 0x52, 0x43}; //QRC - noun that follows is a QR Code 1-bit bmp data stream

//Get 10.5.3 asks to get balance of D67y62BCBcaCGpqnjFjWbUkdXbgBUJzubf
// 
//SO GIB BAL ADR D67y62BCBcaCGpqnjFjWbUkdXbgBUJzubf (infers hub TO)
//--hub sends
//SO WOW BAL AMT 500 KNU 6780000 TO 10.5.3 
//--interpreter looking for msgs for your node would raise:
//MUCH MINE BAL AMT 500 KNU 67800000 FR 10.0.1 (hub addr)

//send a transaction of 200 dogecoin to D67y62BCBcaCGpqnjFjWbUkdXbgBUJzubf from the default wallet:
//SO PAY AMT 200 ADR D67y62BCBcaCGpqnjFjWbUkdXbgBUJzubf (infers hub TO)
//--hub sends
//SO WOW TID (txid) TO 10.5.3
//--interpreter looking for messages for your node would raise:
//MUCH MINE TID (txid) FR 10.0.1 (hub addr)

//send a transaction of 200.53 dogecoin to D67y62BCBcaCGpqnjFjWbUkdXbgBUJzubf from UTXO of 27ffd250b90aa595b5087e3d2afe773373d1d84b044dab96bf7fe78c7f083e6d Index 2
//SO PAY AMT 200 KNU 53000000 ADR D67y62BCBcaCGpqnjFjWbUkdXbgBUJzubf SRC UTX 27ffd250b90aa595b5087e3d2afe773373d1d84b044dab96bf7fe78c7f083e6d IDX 2
//--hub sends
//SO WOW TID (txid) TO 10.5.3


uint8_t rdas_node[3]; // translates to 3 bytes of type nodeAddr
uint8_t rdas_msgid[3]; // 3-byte randomly generated message ID (not currently used)

//message types

//serial commands (as in radioDogeTypes.h)
/*
enum rdas_cmd 
{
	NONE,						//0x00
	ADDRESS_GET,				//0x01
	ADDRESS_SET,				//0x02
	PING_REQUEST,				//0x03
	MESSAGE_REQUEST,			//0x04
	HARDWARE_INFO = 63,			//0x3f = '?'
	HOST_FORMED_PACKET = 104,	//0x68 = 'h'
	RESULT_CODE					//0xFF = what follows is a result code
};

enum rdas_msgtype 
{
	UNKNOWN,					//0x00
	ACK,						//0x01
	PING,						//0x02
	MESSAGE,					//0x03
	NACK						//0x04
};

enum rdas_resultcode
{
	OK = 6,						//0x06 = ascii ack
	ERROR = 21,					//0x15 = ascii nack
};
*/
uint8_t rdas_EOTX = { 0xFF };//end of transmission character (not currently used)

#endif