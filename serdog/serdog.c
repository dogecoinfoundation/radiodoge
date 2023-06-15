// serialDoge 0.01 serialcomms for RadioDoge
//

#include "serdog.h"
#include "consolehelper.h"
#include "fileHelper.h"
#include "libdogecoin.h"
#include <sys/poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <pthread.h>

int pollEnable = 0;

char demo_address1[] = "D6JQ6C48u9yYYarubpzdn2tbfvEq12vqeY";
char demo_address2[] = "DBcR32NXYtFy6p4nzSrnVVyYLjR42VxvwR";
char demo_address3[] = "DGYrGxANmgjcoZ9xJWncHr6fuA6Y1ZQ56Y";
char demoPair1[] = "demo1.txt";
char demoPair2[] = "demo2.txt";
char demoPair3[] = "demo3.txt";
int charsinbuffer = 0;
char loadedDogeAddress[P2PKH_ADDR_STRINGLEN];
char loadedPrivateKey[WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN];
char generatedPrivateKey[WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN];
char destinationDogeAddress[P2PKH_ADDR_STRINGLEN];
uint8_t userPin[PIN_LENGTH] = { 0, 0, 0, 0 };
struct utxoInfo currUTXOs[MAX_NUM_UTXOS];
uint32_t numUTXOs;
char* currentTransaction;
bool demoMode = true;


int openPort()
{
	printf("Trying to open %s...\n", device);
	USB = open(device, O_RDWR | O_NOCTTY); //should be nonblock for polling
	if (USB == -1)
	{
		fprintf(stderr, "Unable to open %s: error= %s\n\n", device, strerror(errno));
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("Open returned file descriptor [%i] with port open status [%s]\n\n", USB, strerror(errno));
	}
}

int init()
{
	memset(&tty, 0, sizeof tty);

	/* Error Handling */
	if (tcgetattr(USB, &tty) != 0)
	{
		printf("Error %i from termios tcGetattr: %s\n", errno, strerror(errno));
	}

	/* Save old tty parameters */
	tty_old = tty;

	/* Set Baud Rate Outgoing */
	if (cfsetospeed(&tty, (speed_t)B115200) != 0)
	{
		printf("Error %i setting output speed: %s\n", errno, strerror(errno));
	}
	else
	{
		printf("Output speed set successfully at %s.\n", printBaudRate(cfgetispeed(&tty)));
	}

	/* Set Baud Rate Outgoing */
	if (cfsetispeed(&tty, (speed_t)B115200) != 0)
	{
		printf("Error %i setting input speed: %s\n", errno, strerror(errno));
	}
	else
	{
		printf("Input speed set successfully at %s.\n\n", printBaudRate(cfgetispeed(&tty)));
	}

	/* Setting other Port Stuff */
	tty.c_cflag &= ~PARENB;            // No parity bit
	tty.c_cflag &= ~CSTOPB;            // 1 stop bit
	tty.c_cflag &= ~CSIZE;             // Mask data size
	tty.c_cflag |= CS8;                // Select 8 data bits

	tty.c_cflag &= ~CRTSCTS;           // no flow control
	tty.c_cc[VMIN] = 1;				   // read doesn't block (1 vmin.)
	tty.c_cc[VTIME] = 0;			   // no read timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // Disable XON/XOFF flow control both i/p and o/p 
	tty.c_oflag &= ~OPOST;//No Output Processing
	// Setting Time outs 


	tty.c_cflag |= (CLOCAL | CREAD);    // turn on READ & ignore ctrl lines

	// Enable data to be processed as raw input
	tty.c_lflag &= ~(ICANON | ECHO | ISIG);


	printf("Terminal parameters set. \n");

	/* Make raw */
	cfmakeraw(&tty);
	printf("Terminal type set to raw. \n");

	/* Flush Port, then applies attributes */
	tcflush(USB, TCIFLUSH);
	if (tcsetattr(USB, TCSANOW, &tty) != 0)
	{
		printf("Error %i from termios tcSetattr: %s\n", errno, strerror(errno));
	}
	else
	{
		printf("Port flush and setup successful.\n\n");
	}
}

int parsePortResponse(uint8_t respCmdType, size_t resplen, char* respbuf)
{
	int n_read = 0;
	int idx = 0;
	char buf = '\0';
	int resptype = 0;

	//get resptype
	if (read(USB, &resptype, 1))
	{
		printf("response type is [%02X]\n", resptype);
		respCmdType = resptype;
	}
	else
	{
		printf("Error getting response type.\n");
		return -1;
	}

	//get resplen
	if (read(USB, &resplen, 1))
	{
		printf("response length is [%02X]\n", (uint8_t)resplen);
	}
	else
	{
		printf("Error getting response length.\n");
		resplen = -1;
		return -1;
	}

	/* Remaining response */
	memset(respbuf, '\0', resplen);
	do {
		n_read = read(USB, &buf, 1);
		{
			printf("nread=%i,read=%02X\n", n_read, buf);
			sprintf(&respbuf[idx], "%c", buf);
		};
		idx += n_read;
	} while (idx < resplen);

	printf("exited read at index %i\n", idx);

	if (n_read < 0)
	{
		printf("Error reading port.\n");
		return -1;
	}
	else if (n_read == 0) {
		printf("Nothing read...\n");
		return 0;
	}
	else
	{
		return sizeof respbuf;
	}
};

/// <summary>
/// Helper function for sending a specific command and payload to the connected radio hardware.
/// </summary>
/// <param name=""></param>
/// <param name="payloadsize"></param>
/// <param name="payload"></param>
/// <returns></returns>
int sendCommand(enum serialCommand cmdtype, int payloadsize, uint8_t* payload)
{
	int n_written = 0;
	int total_written = 0;
	int idx = 0;
	uint8_t currCommand = 0;
	size_t cmdlength = 0;
	uint8_t payloadlen = 0;
	uint8_t txbytes[255 + HDR_LEN]; //payload+hdrlen (cmd+payloadlenbyte)
	int intcmd = cmdtype;
	if (payload != NULL)
	{
		payloadlen = (uint8_t)payloadsize;
		//printf("\nPayload Length %d\n", payloadlen);
	}

	//printf("\nincoming command type %d\n", cmdtype);

	txbytes[0] = (uint8_t)cmdtype;
	txbytes[1] = (uint8_t)payloadlen;

	//printf("cmd type is [%02X]\n", txbytes[0]);
	//printf("embed payload len is [%02X]\n", txbytes[1]);
	if (payload != 0)
	{
		//printf("Payload length %i\n", payloadlen);
		//printf("Payload is :");
		idx = 0;
		do
		{
			//printf(" [%02X] ", payload[idx]);
			txbytes[idx + HDR_LEN] = payload[idx];//+2 because commandtype and payloadlen has been added
			idx++;
		} while (idx != payloadlen);
	}

	//printf("\n");

	//HEADER REDUNDANCY---------------- should be in fw now
	/*
	int re_level = 4; //only 4 supported now 4x repeat

	printf("building in redundancy..\n");
	uint8_t redundant[8 + 255]; //cmd x4 + lenbyte x4 + 255

	for (int r = 0; r < re_level; r++)
	{
		redundant[r]=cmdtype;
	}

	for (int r = 0; r < re_level; r++)
	{
		redundant[r + re_level] = (uint8_t)payloadlen;
	}

	for (int p = 0; p < payloadlen; p++)
	{
		redundant[2 * re_level + p] = payload[p];
	}
	printf("redundant output:");

	idx = 0;
	do
	{
		printf("[%02X]", redundant[idx]);
		idx++;
	} while (idx < (payloadlen + (2 * re_level)));

	printf("\n");

	//HEADER REDUNDANCY----------------
	*/
	idx = 0;
	//printf("Writing bytes: ");
	do {

		//printf("[%02X]", txbytes[idx]);

		n_written = write(USB, &txbytes[idx], 1);
		idx += n_written;

		if (n_written > -1)
		{
			total_written = total_written + n_written;
		}
		else
		{
			printf("\nError writing to port %s as numbytes written was %i.\n", device, n_written);
		}
	} while (total_written != (payloadlen + HDR_LEN) && n_written > 0);
	//printf(" to port [%s]\n", device);

	//printf("[%i] total bytes written to [%s]\n", total_written, device);
}

/// <summary>
/// Sets the local node address of the connected radio hardware
/// </summary>
/// <param name="region"></param>
/// <param name="community"></param>
/// <param name="node"></param>
/// <returns></returns>
int cmdSetLocalAddress(int region, int community, int node)
{
	int cmdtype = NODE_ADDRESS_SET; //we're working with address set for all of this
	uint8_t payload[3] = { (uint8_t)region,(uint8_t)community,(uint8_t)node };
	sendCommand(cmdtype, 3, payload);
}

/// <summary>
/// Request the node address of the connected LoRa module
/// </summary>
/// <returns></returns>
int cmdGetLocalAddress()
{
	//no payload = 0.
	uint8_t cmdtype = NODE_ADDRESS_GET;
	sendCommand(cmdtype, 0, 0);
};

/// <summary>
/// Request hardware information from the connected LoRa module.
/// </summary>
/// <returns></returns>
int cmdGetHardwareInfo()
{
	// No payload for this command
	uint8_t cmdtype = HARDWARE_INFO;
	sendCommand(cmdtype, 0, 0);
}

/// <summary>
/// Request the connected radio module to send a ping to the specified node address.
/// </summary>
/// <param name="inAddr"></param>
/// <returns></returns>
int cmdSendPingCmd(uint8_t* inAddr)
{
	uint8_t cmdtype = PING_REQUEST;
	int payloadsize = 3;
	uint8_t payload[3] = { inAddr[0],inAddr[1],inAddr[2] };
	sendCommand(cmdtype, payloadsize, payload);
};

/// <summary>
/// Sends a custom payload to the desired node
/// </summary>
/// <param name="inAddr">Local node address</param>
/// <param name="destAddr">Destination node address</param>
/// <param name="customPayload">Array containing the custom payload</param>
/// <param name="customPayloadLen">Length of the custom payload</param>
/// <returns></returns>
int cmdSendMessage(uint8_t* inAddr, uint8_t* destAddr, uint8_t* customPayload, uint8_t customPayloadLen)
{
	// Structure of message sent across the air is 1 byte for cmd type, 1 byte for payload length, 3 bytes for sender address, 3 bytes for destination address, then payload
	// Note that the payload length includes the 6 bytes used for addressing
	uint8_t cmdType = HOST_FORMED_PACKET;
	// Combined payload length will be cmd type and payload length + size of the two addresses (6 bytes) + custom payload length
	size_t totalPayloadSize = customPayloadLen + HDR_LEN + ADDR_LEN + ADDR_LEN;
	uint8_t* combinedPayload = malloc(totalPayloadSize);
	combinedPayload[0] = cmdType;
	combinedPayload[1] = totalPayloadSize - HDR_LEN; // Ignore the type and size in this count (i.e. ignore the header)
	int offset = HDR_LEN;
	memcpy(combinedPayload + offset, inAddr, ADDR_LEN);
	offset += ADDR_LEN;
	memcpy(combinedPayload + offset, destAddr, ADDR_LEN);
	offset += ADDR_LEN;
	// We start 8 bytes in since there have now been 2 bytes for the cmd type and payload length in addition to 2 addressed of 3 bytes each
	memcpy(combinedPayload + offset, customPayload, customPayloadLen);
	sendCommand(cmdType, totalPayloadSize, combinedPayload);
	free(combinedPayload);
};

/// <summary>
/// Request the stored Dogecoin address from a specific node.
/// </summary>
/// <param name="inAddr">The connect radio module's node address (Own address)</param>
/// <param name="destAddr">The desired destination node address </param>
cmdRequestDogeAddress(uint8_t* inAddr, uint8_t* destAddr)
{
	uint8_t requestPayload[1] = { GET_DOGE_ADDRESS };
	cmdSendMessage(inAddr, destAddr, requestPayload, 1);
}

/// <summary>
/// Send the specified dogecoin address to another node. 
/// </summary>
/// <param name="inAddr"></param>
/// <param name="destAddr"></param>
/// <param name="dogeAddress"></param>
cmdSendDogeAddress(uint8_t* inAddr, uint8_t* destAddr, char* dogeAddress)
{
	uint8_t payload[P2PKH_ADDR_STRINGLEN + 1];
	payload[0] = SEND_DOGE_ADDRESS;
	memcpy(payload + 1, dogeAddress, P2PKH_ADDR_STRINGLEN);
	cmdSendMessage(inAddr, destAddr, payload, P2PKH_ADDR_STRINGLEN + 1);
}

/// <summary>
/// Request UTXOs for a specific Dogecoin address from another node.
/// Generally UTXOs should be requested from a node that is running as a host and has internet access. 
/// </summary>
/// <param name="inAddr"></param>
/// <param name="destAddr"></param>
/// <param name="dogeAddress"></param>
cmdRequestUTXOs(uint8_t* inAddr, uint8_t* destAddr, char* dogeAddress)
{
	uint8_t payload[P2PKH_ADDR_STRINGLEN + 1];
	payload[0] = REQUEST_UTXOS;
	memcpy(payload + 1, dogeAddress, P2PKH_ADDR_STRINGLEN);
	cmdSendMessage(inAddr, destAddr, payload, P2PKH_ADDR_STRINGLEN + 1);
}

/// <summary>
/// Request the balance of a specific Dogecoin address from another node.
/// Generally balance requests should be sent to a node that is running as a host and has internet access.
/// </summary>
/// <param name="inAddr"></param>
/// <param name="destAddr"></param>
/// <param name="dogeAddress"></param>
cmdRequestBalance(uint8_t* inAddr, uint8_t* destAddr, char* dogeAddress)
{
	uint8_t payload[P2PKH_ADDR_STRINGLEN + 1];
	payload[0] = REQUEST_BALANCE;
	memcpy(payload + 1, dogeAddress, P2PKH_ADDR_STRINGLEN);
	cmdSendMessage(inAddr, destAddr, payload, P2PKH_ADDR_STRINGLEN + 1);
}

/// <summary>
/// Register or remove registration of a Dogecoin address with a host node.
/// </summary>
/// <param name="inaddr"></param>
/// <param name="destAddr"></param>
/// <param name="dogeAddress"></param>
/// <param name="pin"></param>
/// <param name="removeAddress"></param>
cmdRegisterDogeAddress(uint8_t* inaddr, uint8_t* destAddr, char* dogeAddress, uint8_t* pin, bool removeAddress)
{
	// 2 bytes for the registration type and 1 for the registration function to be performed
	int payloadLength = 2 + P2PKH_ADDR_STRINGLEN + PIN_LENGTH;
	uint8_t payload[payloadLength];
	payload[0] = REGISTRATION;
	if (removeAddress)
	{
		payload[1] = REMOVE_REGISTRATION;
	}
	else
	{
		payload[1] = ADD_REGISTRATION;
	}
	memcpy(payload + 2, dogeAddress, P2PKH_ADDR_STRINGLEN);
	memcpy(payload + 2 + P2PKH_ADDR_STRINGLEN, pin, PIN_LENGTH);
	cmdSendMessage(inaddr, destAddr, payload, payloadLength);
}

/// <summary>
/// Send a raw (signed) Dogecoin transaction to a host node for transmission on the Dogecoin network.
/// </summary>
/// <param name="inaddr"></param>
/// <param name="destAddr"></param>
/// <param name="rawTransaction"></param>
/// <param name="requestId"></param>
cmdSendTransaction(uint8_t* inaddr, uint8_t* destAddr, char* rawTransaction, uint8_t requestId)
{
	int transactionLength = strlen(rawTransaction);
	int payloadLength = 1 + transactionLength;
	uint8_t payload[payloadLength];
	payload[0] = TRANSACTION_REQUEST;
	memcpy(payload + 1, rawTransaction, transactionLength);
	cmdSendMultipartMessage(inaddr, destAddr, payload, payloadLength, requestId);
}


/// <summary>
/// Update the registered pin stored at a host node.
/// </summary>
/// <param name="inaddr"></param>
/// <param name="destAddr"></param>
/// <param name="dogeAddress"></param>
/// <param name="oldPin"></param>
/// <param name="updatedPin"></param>
cmdUpdateRegistrationPin(uint8_t* inaddr, uint8_t* destAddr, char* dogeAddress, uint8_t* oldPin, uint8_t* updatedPin)
{
	int payloadLength = 2 + P2PKH_ADDR_STRINGLEN + (2 * PIN_LENGTH);
	uint8_t payload[payloadLength];
	payload[0] = REGISTRATION;
	payload[1] = UPDATE_PIN;
	int offset = 2;
	memcpy(payload + offset, dogeAddress, P2PKH_ADDR_STRINGLEN);
	offset += P2PKH_ADDR_STRINGLEN;
	memcpy(payload + offset, oldPin, PIN_LENGTH);
	offset += PIN_LENGTH;
	memcpy(payload + offset, updatedPin, PIN_LENGTH);
	cmdSendMessage(inaddr, destAddr, payload, payloadLength);
}

/// <summary>
/// Send a multipart packet/message to a specific node.
/// </summary>
/// <param name="inAddr"></param>
/// <param name="destAddr"></param>
/// <param name="customPayload"></param>
/// <param name="customPayloadLen"></param>
/// <param name="messageID"></param>
/// <returns></returns>
int cmdSendMultipartMessage(uint8_t* inAddr, uint8_t* destAddr, uint8_t* customPayload, int customPayloadLen, uint8_t messageID)
{
	uint8_t cmdType = MULTIPART_PACKET;
	int totalNumParts = customPayloadLen / MAX_PAYLOAD_LEN;
	if (customPayloadLen % MAX_PAYLOAD_LEN != 0)
	{
		totalNumParts++;
	}
	size_t maxPacketSize = MAX_PAYLOAD_LEN + MULTIPART_HDR_LEN;
	printf("Sending message in %d parts\n", totalNumParts);
	int payloadLeft = customPayloadLen;
	int payloadInd = 0;
	// Each payload part will have a cmd type (1 byte), payload length (1 byte), 2 addresses (3 bytes each), and 4 bytes for multipart info 
	uint8_t currentPacket[maxPacketSize];
	int currPayloadLen = 0;
	for (int i = 1; i < totalNumParts + 1; i++)
	{
		currentPacket[0] = cmdType;
		if (payloadLeft >= MAX_PAYLOAD_LEN)
		{
			currPayloadLen = MAX_PAYLOAD_LEN;
		}
		else
		{
			currPayloadLen = payloadLeft;

		}
		currentPacket[1] = currPayloadLen + MULTIPART_HDR_LEN - HDR_LEN;
		int offset = HDR_LEN;
		memcpy(currentPacket + offset, inAddr, ADDR_LEN);
		offset += ADDR_LEN;
		memcpy(currentPacket + offset, destAddr, ADDR_LEN);
		offset += ADDR_LEN;
		// Now include multipart info
		currentPacket[offset] = messageID;
		currentPacket[offset + 1] = 0; // for now we will reserve this one byte
		currentPacket[offset + 2] = (uint8_t)(i);
		currentPacket[offset + 3] = (uint8_t)totalNumParts;
		offset += MULTIPART_PIECE_INFO_LEN;
		memcpy(currentPacket + offset, customPayload + payloadInd, currPayloadLen);
		sendCommand(cmdType, currPayloadLen + MULTIPART_HDR_LEN, currentPacket);
		payloadInd += currPayloadLen;
		payloadLeft -= currPayloadLen;
		// Delay each part of the packet
		sleep(1);
	}
}

/// <summary>
/// Deobfuscate a Dogecoin balance received over the air
/// </summary>
/// <param name="pin"></param>
/// <param name="serializedBalance"></param>
/// <returns></returns>
uint64_t deobfuscateReceivedBalance(uint8_t* pin, uint8_t* serializedBalance)
{
	for (int i = 0; i < SERIALIZED_BALANCE_LENGTH; i++)
	{
		serializedBalance[i] ^= pin[i % PIN_LENGTH];
	}
	uint64_t balance;
	printByteArray(serializedBalance, sizeof(balance));
	memcpy(&balance, serializedBalance, sizeof(balance));
	return balance;
}

/// <summary>
/// Deserialize UTXOs received over the air and store them for later use
/// </summary>
/// <param name="serializedUTXOs"></param>
void deserializeUTXOs(uint8_t* serializedUTXOs)
{
	int currOffset = 0;
	for (int i = 0; i < numUTXOs; i++)
	{
		// Extract the vout value
		memcpy(&currUTXOs[i].vout, serializedUTXOs + currOffset, SERIALIZED_VOUT_LENGTH);
		currOffset += SERIALIZED_VOUT_LENGTH;
		// Extract the TXID string
		memcpy(currUTXOs[i].txId, serializedUTXOs + currOffset, TXID_STRING_LENGTH);
		currOffset += TXID_STRING_LENGTH;
		// Extract the amount
		printByteArray(serializedUTXOs + currOffset, SERIALIZED_BALANCE_LENGTH);
		memcpy(&currUTXOs[i].amount, serializedUTXOs + currOffset, SERIALIZED_BALANCE_LENGTH);
		currOffset += SERIALIZED_BALANCE_LENGTH;
	}
}

/// <summary>
/// Display all of the currently stored UTXOs
/// </summary>
void printAllUTXOs()
{
	if (numUTXOs > 0)
	{
		for (int i = 0; i < numUTXOs; i++)
		{
			printf("\n### UTXO %i ###\n", i);
			printf("TXID: %s\n", currUTXOs[i].txId);
			printf("Vout: %i\n", currUTXOs[i].vout);
			char coinString[21];
			koinu_to_coins_str(currUTXOs[i].amount, coinString);
			printf("Amount (Coins): %s\n", coinString);
			printf("Amount (Koinu): %lu\n", currUTXOs[i].amount);
		}
	}
	else
	{
		printf("There currently are no stored UTXOs!\n");
	}
}

/// <summary>
/// Processes UTXOs received over the air from another node
/// </summary>
/// <param name="payloadIn"></param>
void processReceivedUTXOs(uint8_t* payloadIn)
{
	//First 4 bytes are the number of UTXOs serialized
	memcpy(&numUTXOs, payloadIn, sizeof(numUTXOs));
	printf("Received %i UTXOs!\n", numUTXOs);

	// The rest will be serialized UTXOs so we need to deserialize
	deserializeUTXOs(payloadIn + SERIALIZED_NUM_UTXO_LENGTH);
	printAllUTXOs();
}

/// <summary>
/// Get user input to manually add a UTXO
/// </summary>
void manuallyAddUTXO()
{
	if (numUTXOs == MAX_NUM_UTXOS)
	{
		printf("The UTXO storage is currently full! A new UTXO can't be added!\n");
		return;
	}
	printf("Manually adding a UTXO\n");
	// Get the TXID
	printf("Please enter the TXID:\n");
	scanf("%s", currUTXOs[numUTXOs].txId);
	// Get the Vout
	printf("Enter the vout:\n");
	int vout_success = scanf("%d", &currUTXOs[numUTXOs].vout);
	if (!vout_success)
	{
		printf("Invalid vout value!\n");
		return;
	}
	// Get the amount
	printf("Enter the UTXO amount:\n");
	char tempAmount[21];
	scanf("%s", tempAmount);
	currUTXOs[numUTXOs].amount = coins_to_koinu_str(tempAmount);

	// Increase the number of UTXOS
	numUTXOs++;
}

/// <summary>
/// Enter into manual UTXO editing mode.
/// This mode allows users to clear, add, and display stored UTXOs
/// </summary>
void enterUTXOsEditingMode()
{
	int userSelection = 0;
	while (userSelection >= 0)
	{
		userSelection = getManualUtxosEditingSelection();
		switch (userSelection)
		{
		// Clear UTXOs
		case 1:
			// Just setting this to 0 will essentially "clear" the UTXO storage as any additional UTXOs will overwrite old ones
			numUTXOs = 0;
			printf("The stored UTXOs have been cleared!\n");
			break;
		// Add UTXO
		case 2:
			manuallyAddUTXO();
			break;
		//Display UTXOs
		case 3:
			printAllUTXOs();
			break;
		}
	}
}

/// <summary>
/// Process received transaction results sent from another node
/// </summary>
/// <param name="payloadIn"></param>
/// <param name="payloadSize"></param>
void processTransactionResult(uint8_t* payloadIn, int payloadSize)
{
	if (payloadSize == 2)
	{
		// This is a failed transaction
		printf("Transaction failed! TXID: %c\n", payloadIn[0]);
	}
	else if (payloadSize == 1 + TXID_STRING_LENGTH)
	{
		char receivedTXID[TXID_STRING_LENGTH + 1];
		receivedTXID[TXID_STRING_LENGTH] = '\0';
		memcpy(receivedTXID, payloadIn, TXID_STRING_LENGTH);
		printf("Result TXID: %s\n", receivedTXID);
	}
	else
	{
		printf("Unknown transaction result!\n");
	}

}

/// <summary>
/// Create a raw (signed) Dogecoin transaction.
/// </summary>
/// <returns></returns>
bool createTransaction()
{
	printf("*** Creating a transaction to send dogecoin ***\n");
	printf("Sender: %s\n", loadedDogeAddress);
	printf("Destination: %s\n", destinationDogeAddress);

	// Get input from user on how much to send
	char amount_to_send[MAX_DOGECOIN_AMOUNT_STRING_LENGTH];
	getUserSuppliedDogecoinAmount(amount_to_send);
	printf("Amount to send: %s\n", amount_to_send);
	uint64_t desired_amount_koinu = coins_to_koinu_str(amount_to_send);

	// Add the fixed fee into the desired amount so that we know the total needed to complete the transaction
	uint64_t desired_fee_koinu = coins_to_koinu_str("1.0");
	desired_amount_koinu += desired_fee_koinu;

	// Check that there is at least one utxo stored
	if (numUTXOs < 1)
	{
		printf("There are no UTXOs stored for %s\n", loadedDogeAddress);
		return false;
	}

	int curr_tx_index = start_transaction();

	// Add all available utxos until we exceed the needed amount
	uint64_t utxo_total_amount = 0;
	int numUTXOsUsed = 0;
	for (int i = 0; i < numUTXOs; i++)
	{
		int add_utxo_result = add_utxo(curr_tx_index, currUTXOs[i].txId, currUTXOs[i].vout);
		utxo_total_amount += currUTXOs[i].amount;
		numUTXOsUsed++;
		if (!add_utxo_result)
		{
			printf("Error adding UTXO %i!\n", i);
			clear_transaction(curr_tx_index);
			return false;
		}

		// Check to see if we added enough UTXOs to satisfy the desired amount
		// This way we aren't including more utxos than needed
		if (utxo_total_amount >= desired_amount_koinu)
		{
			break;
		}
	}

	printf("%i UTXOs were added to the transaction!\n", numUTXOsUsed);

	char utxo_total_amount_str[MAX_DOGECOIN_AMOUNT_STRING_LENGTH];
	int conversion_result = koinu_to_coins_str(utxo_total_amount, utxo_total_amount_str);
	printf("UTXO total amount: %s\n", utxo_total_amount_str);

	// Check to make sure all the UTXOs we added up equal or exceed the desired amount
	if (utxo_total_amount < desired_amount_koinu)
	{
		printf("The balance of the wallet does not contain enough Dogecoin to complete the transaction!\n");
		koinu_to_coins_str(desired_amount_koinu, amount_to_send);
		printf("Needed: %s\n", amount_to_send);
		printf("Added: %s\n", utxo_total_amount_str);
		clear_transaction(curr_tx_index);
		return false;
	}

	// Add output
	int output_result = add_output(curr_tx_index, destinationDogeAddress, amount_to_send);
	if (!output_result)
	{
		printf("Error adding output!\n");
		clear_transaction(curr_tx_index);
		return false;
	}

	// For now we will charge a fixed fee of 1.0 dogecoin
	// Finalize the transaction
	finalize_transaction(curr_tx_index, destinationDogeAddress, "1.0", utxo_total_amount_str, loadedDogeAddress);

	// Sign the transaction for each UTXO used
	for (int i = 0; i < numUTXOsUsed; i++)
	{
		if (!sign_transaction_w_privkey(curr_tx_index, currUTXOs[i].vout, loadedPrivateKey))
		{
			printf("Failed to sign transaction for UTXO with vout=%i\n", i);
			clear_transaction(curr_tx_index);
			return false;
		}
	}
	currentTransaction = get_raw_transaction(curr_tx_index);
	clear_transaction(curr_tx_index);
	return true;
}

void* serialPollThread(void* threadid)
{
	uint8_t rxbuffer[1024] = { 0 };
	uint8_t rxindex = 0;
	int* thisid = (int*)threadid;
	printf("Receive monitor thread started with ID: %d\n", thisid);

	struct pollfd serFileDescriptor[1];
	serFileDescriptor[0].fd = USB;
	serFileDescriptor[0].events = POLLIN;

	ssize_t totalRxChars = 0;
	int rxPayloadSize = -1;

	// Single packet payload storage
	uint8_t dataBuffer[255] = { 0 };

	// Multipart payload related variables
	uint8_t multipartBuffer[4096] = { 0 };
	int multipartIndex = 0;
	uint8_t senderAddress[3] = { 0 };

	do
	{
		int serReceivedCharacters = poll(serFileDescriptor, 1, 1000);

		if (serReceivedCharacters < 0)
		{
			//perror("poll error");
			printf("*** THREAD: error in poll thread \n", NULL);
		}
		else if (serReceivedCharacters > 0)
		{
			uint8_t minibuffer[255] = { 0 };
			if (serFileDescriptor[0].revents & POLLIN)
			{

				ssize_t receivedCharactersLen = read(USB, minibuffer, sizeof(minibuffer));
				if (receivedCharactersLen > 0)
				{
					//printf("*** THREAD: Received %i chars\n", receivedCharactersLen);

					for (int rc = 0; rc < receivedCharactersLen; rc++)
					{
						//printf("[%02x]", (uint8_t)minibuffer[rc]);
						rxbuffer[totalRxChars] = minibuffer[rc];
						totalRxChars++;
					}

					//printf("\n", NULL);
				}
			}
		}

		// Make sure we actually received some characters and transferred them into the receive buffer before trying to parse
		if (totalRxChars > 0)
		{
			rxPayloadSize = isCompleteCmd(rxbuffer, totalRxChars);
			if (rxPayloadSize >= 0)
			{
				//printf("**Valid and complete cmd** totRX=%i psize=%i\n", totalRxChars, rxPayloadSize);
				if (rxbuffer[0] == MULTIPART_PACKET)
				{
					//printf("**Partial Payload (with packet header)**\n");
					//printByteArray(rxbuffer, totalRxChars);
					int processResult = parseMultipartPayload(rxbuffer, totalRxChars, multipartBuffer, &multipartIndex, senderAddress);
					if (processResult)
					{

						printf("Fully reassembled payload!\n");
						printNodeAddress("Sender", senderAddress);
						printByteArray(multipartBuffer, multipartIndex);
						processDogePayload(senderAddress, multipartBuffer, multipartIndex);

						// Reset multipart count values
						multipartIndex = 0;
					}
				}
				else if (rxbuffer[0] == HOST_FORMED_PACKET)
				{
					printf("**Complete Payload (with packet header)**\n");
					printByteArray(rxbuffer, totalRxChars);
					parseHostFormedPacket(senderAddress, dataBuffer, rxbuffer, totalRxChars);
					// @TODO do something else with the payload
					processDogePayload(senderAddress, dataBuffer, totalRxChars - SINGLE_PACKET_HDR_LEN);
				}
				else
				{
					// Parse as a cmd payload
					processCommandPayload(rxbuffer, totalRxChars);
				}

				// At this point we processed the payload
				// Therefore we will reset the buffer's received character size to 0 allowing it to be rewritten on next receive
				totalRxChars = 0;
			}
			else
			{
				//printf("***Not cmd** totRX=%i\n", totalRxChars);
			}
		}
	} while (pollEnable == 1);
	printf("*** THREAD: Exiting thread %d ..\n", thisid);
	pthread_exit(0);
}

void parseHostFormedPacket(uint8_t* senderAddr, uint8_t* extractedDataBuffer, uint8_t* payloadIn, int payloadSize)
{
	printf("Received host formed packet!\n");
	// Strip off the sender and dest address
	int offset = HDR_LEN;
	memcpy(senderAddr, payloadIn + HDR_LEN, ADDR_LEN);
	offset += ADDR_LEN;
	uint8_t destAddr[3];
	memcpy(destAddr, payloadIn + offset, ADDR_LEN);
	offset += ADDR_LEN;
	// Isolate the payload
	int dataLen = payloadSize - offset;
	memcpy(extractedDataBuffer, payloadIn + offset, dataLen);
	printNodeAddress("Sender", senderAddr);
	printNodeAddress("Destination", destAddr);
	printf("Data:\n");
	printByteArray(extractedDataBuffer, dataLen);
}

int isCompleteCmd(uint8_t* inBuf, int charsReceived)
{
	if (charsReceived >= HDR_LEN)
	{
		//printf("int inbuf %i %i %i\n", (int)inBuf[0], (int)inBuf[1], (int)inBuf[2]);
		if (isCmd(inBuf[0]) && (int)inBuf[1] == charsReceived - HDR_LEN)
		{
			//printf("charsReceived: %i\n", charsReceived);
			return ((int)inBuf[1]);
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}
}

int isCmd(uint8_t inByte)
{
	switch ((int)inByte)
	{
	case NODE_ADDRESS_GET:
		return (int)NODE_ADDRESS_GET;
		break;
	case NODE_ADDRESS_SET:
		return (int)NODE_ADDRESS_SET;
		break;
	case PING_REQUEST:
		return (int)PING_REQUEST;
		break;
	case MESSAGE_REQUEST:
		return (int)MESSAGE_REQUEST;
		break;
	case HARDWARE_INFO:
		return (int)HARDWARE_INFO;
		break;
	case HOST_FORMED_PACKET:
		return (int)HOST_FORMED_PACKET;
		break;
	case MULTIPART_PACKET:
		return (int)MULTIPART_PACKET;
		break;
	case RESULT_CODE:
		return (int)RESULT_CODE;
		break;

	default:
		return 0;
	}
}

/// <summary>
/// Process a received Doge payload received over the air from a different node
/// </summary>
/// <param name="senderAddr"></param>
/// <param name="payloadIn"></param>
/// <param name="payloadSize"></param>
void processDogePayload(uint8_t* senderAddr, uint8_t* payloadIn, int payloadSize)
{
	switch (payloadIn[0])
	{
	case GET_DOGE_ADDRESS:
		printf("Send Doge Address Request received!\n");
		sendDogeAddressTest(senderAddr);
		break;
	case SEND_DOGE_ADDRESS:
		printf("Received Doge Address!\n");
		// Printing received Dogecoin address (1st byte in this payload in the opcode)
		printf("%s\n", payloadIn + 1);
		break;
	case BALANCE_RECEIVED:
		printf("Received Dogecoin Balance!\n");
		uint64_t balanceReceived = deobfuscateReceivedBalance(userPin, payloadIn + 1);
		// Now turn balance into a float
		float balanceFloat = (float)balanceReceived / (float)100000000;
		printf("Balance: %f\n", balanceFloat);
		break;
	case UTXOS_RECEIVED:
		// We supply payloadIn + 1 here since the first byte will just be the Command Type (UTXOS_RECEIVED)
		processReceivedUTXOs(payloadIn + 1);
		break;
	case DOGE_COMMAND_SUCCESS:
		printf("RadioDoge Hub node executed command successfully!\n");
		// @TODO provide more info to user
		break;
	case DOGE_COMMAND_FAILURE:
		printf("ERROR: RadioDoge Hub node failed to execute command!\n");
		// @TODO failure reasoning...
		break;
	case TRANSACTION_RESULT:
		printf("Transaction result received!\n");
		processTransactionResult(payloadIn + 1, payloadSize);
		break;
	default:
		printf("Unknown payload received!\n");
		break;
	}
}

/// <summary>
/// Process a command and control related payload received from connected radio hardware 
/// </summary>
/// <param name="payloadIn"></param>
/// <param name="payloadSize"></param>
void processCommandPayload(uint8_t* payloadIn, int payloadSize)
{
	switch (payloadIn[0])
	{
	case NODE_ADDRESS_GET:
		printf("Local node address: %i.%i.%i\n", payloadIn[2], payloadIn[3], payloadIn[4]);
		break;
	case NODE_ADDRESS_SET:
		// Should not see this
		// This command will just be ACK'd by the module
		break;
	case PING_REQUEST:
		// Should not see this
		// This command will just be ACK'd by the module without host intervention
		break;
	case MESSAGE_REQUEST:
		break;
	case HARDWARE_INFO:
		printf("Hardware info received from device!\n");
		if ((char)payloadIn[2] == 'h')
		{
			printf("Heltec WiFi LoRa 32 (V%i) - Firmware version %i\n", payloadIn[3], payloadIn[4]);
		}
		else
		{
			printf("Unknown hardware!\n");
		}
		break;
	case HOST_FORMED_PACKET:
		printf("ERROR: Received host formed packet when a command packet was expected\n");
		break;
	case MULTIPART_PACKET:
		printf("ERROR: Received multipart packet when a command packet was expected!\n");
		break;
	case RESULT_CODE:
		if (payloadIn[2] == RESULT_ACK)
		{
			printf("Device ACK'd command!\n");
		}
		else if (payloadIn[2] == RESULT_NACK)
		{
			printf("ERROR: Device sent a NACK!\n");
		}
		break;

	default:
		printf("Unknown payload!");
	}
}

/// <summary>
/// Reassemble a multipart payload received over the air from another node
/// </summary>
/// <param name="payloadPartIn"></param>
/// <param name="partSize"></param>
/// <param name="multipartBuffer"></param>
/// <param name="multipartSize"></param>
/// <param name="senderAddress"></param>
/// <returns></returns>
int parseMultipartPayload(uint8_t* payloadPartIn, int partSize, uint8_t* multipartBuffer, int* multipartSize, uint8_t* senderAddress)
{
	// @TODO 
	// NOTE: this will not work if we miss a packet. Will need to add in smarter piece tracking
	// Also there will be issues if we receive multipart packets from multiple nodes simultaneously as it doesn't do anything with the msg id
	// This just reassembles the pieces for now
	if (*multipartSize == 0)
	{
		// Save the sender address
		senderAddress[0] = payloadPartIn[2];
		senderAddress[1] = payloadPartIn[3];
		senderAddress[2] = payloadPartIn[4];
	}
	// Copy just the data portion over to the buffer
	// This requires stripping out the header portion which will be 12 bytes
	memcpy(multipartBuffer + *multipartSize, payloadPartIn + MULTIPART_HDR_LEN, partSize - MULTIPART_HDR_LEN);
	*multipartSize += (partSize - MULTIPART_HDR_LEN);

	// Check if it was the last part
	// We do this currently by seeing if the piece number is equal to the total number of pieces
	return payloadPartIn[10] == payloadPartIn[11];
}

void waitForResponse(enum serialCommand cmdtype)
{
	uint8_t rxbuffer[1024] = { 0 };
	do
	{
		usleep(5000);
	} while (charsinbuffer < 1);

	printf("Relayed out of thread:");
	for (int rc = 0; rc < charsinbuffer; rc++)
	{
		printf("[%02x]", (uint8_t)rxbuffer[rc]);
	}
	printf("\n");
}

int testLib(char* addrbuffer)
{
	//create a buffer the size of a private key (wallet import format uncompressed key length)
	//this constant is in include/constants.h, included via libdogecoin.h
	char keybuffer[WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN];

	//create a string to compare to; valid dogecoin addresses generated this way start with "D".
	char* addrheader = "D";

	//Generate a private key (WIF format) and a public key (p2pkh dogecoin address) for the main net.
	generatePrivPubKeypair(keybuffer, addrbuffer, false);
	//If the returned address starts with "d" then return good/true/"1".
	return (1-(strncmp(addrbuffer, addrheader, 1)));
}

int sendDogeAddressTest(uint8_t* destAddr)
{
	//set up a buffer string the size of a dogecoin address (P2PKH address) - in include/constants.h
	char addrbuffer[P2PKH_ADDR_STRINGLEN];
	createTestDogeAddress(addrbuffer, generatedPrivateKey);
	printf("Sending Test Address: % s \n", addrbuffer);
	cmdSendDogeAddress(myaddr, destAddr, addrbuffer);
}

/// <summary>
/// Display a QR code for the specified Dogecoin address
/// </summary>
/// <param name="dogeAddress"></param>
void displayDogeQRCode(char* dogeAddress)
{
	//set up a buffer string the size of a dogecoin address (P2PKH address) - in include/constants.h
	char qrBuffer[4096];
	int result = qrgen_p2pkh_to_qr_string(dogeAddress, qrBuffer);
	printf("Dogecoin Address: % s\n", dogeAddress);
	printf("%s\n", qrBuffer);
}

/// <summary>
/// Create a new Dogecoin address for testing purposes (pub & priv keys are generated)
/// </summary>
/// <param name="dogeAddress"></param>
/// <param name="generatedPrivKey"></param>
void createTestDogeAddress(char* dogeAddress, char* generatedPrivKey)
{
	printf("Generating a new test DogeCoin address!\n");
	//Generate a private key (WIF format) and a public key (p2pkh dogecoin address) for the main net.
	generatePrivPubKeypair(generatedPrivKey, dogeAddress, false);
}

void updateRegisteredPin()
{
	// Update Registered Pin
	uint8_t updatedPin[PIN_LENGTH];
	getUserSuppliedPin(updatedPin);
	cmdUpdateRegistrationPin(myaddr, rmaddr, loadedDogeAddress, userPin, updatedPin);
	for (int i = 0; i < PIN_LENGTH; i++)
	{
		userPin[i] = updatedPin[i];
	}
}

void modeSelectionLoop()
{
	int selectedMode = 0;
	while (selectedMode >= 0)
	{
		selectedMode = getModeSelection();
		switch (selectedMode)
		{
		case 1:
			// Enter Setup mode
			enterSetupMode();
			break;
		case 2:
			// Enter Doge mode
			enterDogeMode();
			break;
		case 3:
			// Enter Test mode
			enterTestMode();
			break;
		}
	}
}

void enterSetupMode()
{
	int userSelection = 0;
	while (userSelection >= 0)
	{
		userSelection = getSetupModeSelection();
		switch (userSelection)
		{
		case 1:
			// Get node address
			cmdGetLocalAddress();
			break;
		case 2:
			// Set node address
			// Get user supplied node address
			getUserSuppliedNodeAddress(myaddr);
			cmdSetLocalAddress(myaddr[0], myaddr[1], myaddr[2]);
			break;
		case 3:
			// Set destination node address
			// Get user supplied node address
			getUserSuppliedNodeAddress(rmaddr);
			printNodeAddress("Destination", rmaddr);
			break;
		case 4:
			// Send ping
			printNodeAddress("Sending Ping To", rmaddr);
			cmdSendPingCmd(rmaddr);
			break;
		case 5:
			// Send Message
			printf("Sending messages is not currently supported!\n");
			break;
		case 6:
			// Get Hardware information
			cmdGetHardwareInfo();
			break;
		}
		sleep(2);
	}
}

void enterDogeMode()
{
	int userSelection = 0;
	while (userSelection >= 0)
	{
		userSelection = getDogeModeSelection();
		switch (userSelection)
		{
		case 1:
			// Get Dogecoin Address
			cmdRequestDogeAddress(myaddr, rmaddr);
			break;
		case 2:
			// Get Dogecoin Balance
			cmdRequestBalance(myaddr, rmaddr, loadedDogeAddress);
			break;
		case 3:
			// Get UTXOs
			cmdRequestUTXOs(myaddr, rmaddr, loadedDogeAddress);
			break;
		case 4:
			// Send Dogecoin
			if (createTransaction())
			{
				uint8_t transactionId = 123; // @TODO set this to something unique?
				printf("Raw Transaction: %s\n", currentTransaction);
				cmdSendTransaction(myaddr, rmaddr, currentTransaction, transactionId);
			}
			else
			{
				printf("Failed to create a transaction!\n");
			}
			break;
		case 5:
			// Display QR code
			displayDogeQRCode(loadedDogeAddress);
			break;
		case 6:
			// Register Address
			getUserSuppliedPin(userPin);
			cmdRegisterDogeAddress(myaddr, rmaddr, loadedDogeAddress, userPin, false);
			break;
		case 7:
			// Remove Address Registration
			cmdRegisterDogeAddress(myaddr, rmaddr, loadedDogeAddress, userPin, true);
			break;
		case 8:
			// Update Registered Pin
			updateRegisteredPin();
			break;
		case 9:
			// Load demo address pair
			LoadDemoAddressPair();
			printf("Currently loaded Dogecoin address: %s\n", loadedDogeAddress);
			break;
		case 10:
			// Load Destination Demo Address
			LoadDestinationAddress(destinationDogeAddress);
			printf("Currently loaded Destination Dogecoin address: %s\n", destinationDogeAddress);
			break;
		case 11:
			printf("Current pin: ");
			for (int i = 0; i < PIN_LENGTH; i++)
			{
				printf("%i", userPin[i]);
			}
			printf("\n");
			getUserSuppliedPin(userPin);
			printf("Updated pin: ");
			for (int i = 0; i < PIN_LENGTH; i++)
			{
				printf("%i", userPin[i]);
			}
			printf("\n");
			break;
		case 12:
			enterUTXOsEditingMode();
			break;
		}
		sleep(2);
	}
}

/// <summary>
/// Get user input to specify a destination Dogecoin address
/// </summary>
/// <param name="addressBuffer"></param>
void LoadDestinationAddress(char* addressBuffer)
{
	int addressSelection = getDemoAddressSelection();
	switch (addressSelection)
	{
	case 1:
		strcpy(addressBuffer, demo_address1);
		break;
	case 2:
		strcpy(addressBuffer, demo_address2);
		break;
	case 3:
		strcpy(addressBuffer, demo_address3);
		break;
	case 4:
		createTestDogeAddress(addressBuffer, generatedPrivateKey);
		break;
	default:
		printf("No demo address loaded!\n");
		break;
	}
}

void LoadDemoAddressPair()
{
	int addressSelection = getDemoAddressSelection();
	switch (addressSelection)
	{
	case 1:
		loadDogecoinAddress(demoPair1, loadedDogeAddress, loadedPrivateKey);
		break;
	case 2:
		loadDogecoinAddress(demoPair2, loadedDogeAddress, loadedPrivateKey);
		break;
	case 3:
		loadDogecoinAddress(demoPair3, loadedDogeAddress, loadedPrivateKey);
		break;
	case 4:
		createTestDogeAddress(loadedDogeAddress, loadedPrivateKey);
		break;
	}

	// @TODO remove. Only used right now for debugging purposes
	//printf("Loaded...\nAddress: %s\nPrivate Key: %s\n", loadedDogeAddress, loadedPrivateKey);
}

void enterTestMode()
{
	int userSelection = 0;
	while (userSelection >= 0)
	{
		userSelection = getTestModeSelection();
		switch (userSelection)
		{
		case 1:
			// Send Packet Test
			printf("Not currently supported!\n");
			break;
		case 2:
			// Multipart Packet Test
			multipartCountTest();
			break;
		case 3:
			// Display Control Test
			displayControlTest();
			break;
		}
		sleep(2);
	}
}

/// <summary>
/// Testing function for controlling the connected LoRa module/radio hardware's display
/// </summary>
void displayControlTest()
{
	// No payload for this command
	uint8_t cmdtype = DISPLAY_CONTROL;
	uint8_t payload[1] = { (uint8_t)RADIO_DOGE_LOGO };
	printf("Displaying Radio Doge Logo\n");
	sendCommand(cmdtype, 1, payload);
	sleep(2);
	payload[0] = (uint8_t)DOGE_ANIMATION;
	printf("Displaying Doge Animation\n");
	sendCommand(cmdtype, 1, payload);
	sleep(5);
	payload[0] = (uint8_t)COIN_ANIMATION;
	printf("Displaying Coin Animation\n");
	sendCommand(cmdtype, 1, payload);
	sleep(5);
	payload[0] = (uint8_t)RADIO_DOGE_LOGO;
	printf("Displaying Radio Doge Logo\n");
	sendCommand(cmdtype, 1, payload);
	sleep(2);
}

void multipartCountTest()
{
	int custSize = 1024;
	uint8_t customTestPayload[custSize];
	for (int i = 0; i < custSize; i++)
	{
		customTestPayload[i] = (uint8_t)(i % 256);
	}
	printf("\nCustom Payload:\n");
	printByteArray(customTestPayload, custSize);
	//cmdSendMessage(myaddr, rmaddr, customTestPayload, custSize);
	cmdSendMultipartMessage(myaddr, rmaddr, customTestPayload, custSize, 123);
}

void hardwareTests()
{
	//setLocalAddr
	printNodeAddress("CMD TEST: Setting Local ", myaddr);
	cmdSetLocalAddress(myaddr[0], myaddr[1], myaddr[2]);
	//waitForResponse(ADDRESS_SET);

	getc(stdin);// Wait for keypress for next command
	printf("Sending Next Command!\n");

	//queryLocalAddr
	printf("CMD TEST: Querying Local Address...\n");
	cmdGetLocalAddress();
	//printf("\nFrom command, received: \n[%02X][%02X][%02X]\n\n", rxbuf[0],rxbuf[1],rxbuf[2]);

	// Get hardware info
	getc(stdin);// Wait for keypress for next command
	printf("CMD TEST: Getting hardware info...\n");
	cmdGetHardwareInfo();

	getc(stdin);// Wait for keypress for next command
	multipartCountTest();
}

void fileWriteReadTest()
{
	printf("Performing address read write test");
	// First create a test address
	//create a buffer the size of a private key (wallet import format uncompressed key length)
	//this constant is in include/constants.h, included via libdogecoin.h
	char keybuffer[WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN];
	char testAddress[P2PKH_ADDR_STRINGLEN];
	//Generate a private key (WIF format) and a public key (p2pkh dogecoin address) for the main net.
	generatePrivPubKeypair(keybuffer, testAddress, false);

	// Write to a file
	printf("Saving...\nAddress: %s\n Private Key: %s\n", testAddress, keybuffer);
	saveDogecoinAddress("savedAddressTest.txt", testAddress, keybuffer);

	// Load from the file
	char loadedKey[WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN];
	char loadedAddress[P2PKH_ADDR_STRINGLEN];
	loadDogecoinAddress("savedAddressTest.txt", loadedAddress, loadedKey);
	printf("Loaded...\nAddress: %s\nPrivate Key: %s\n", loadedAddress, loadedKey);
}

void loadFakeUTXOs()
{
	printf("Loading Fake UTXOs for testing purposes...\n");

	uint64_t amount0 = coins_to_koinu_str("10.5");
	currUTXOs[0].amount = amount0;
	currUTXOs[0].txId[0] = '0';
	currUTXOs[0].vout = 0;
	
	uint64_t amount1 = coins_to_koinu_str("50.15");
	currUTXOs[1].amount = amount1;
	currUTXOs[1].txId[0] = '1';
	currUTXOs[1].vout = 1;

	uint64_t amount2 = coins_to_koinu_str("12.25");
	currUTXOs[2].amount = amount2;
	currUTXOs[2].txId[0] = '2';
	currUTXOs[2].vout = 2;

	uint64_t amount3 = coins_to_koinu_str("3");
	currUTXOs[3].amount = amount3;
	currUTXOs[3].txId[0] = '3';
	currUTXOs[3].vout = 3;

	uint64_t amount4 = coins_to_koinu_str("4");
	currUTXOs[4].amount = amount4;
	currUTXOs[4].txId[0] = '4';
	currUTXOs[4].vout = 4;

	numUTXOs = 5;

	printAllUTXOs();
}

void generateDemoPairFiles()
{
	printf("Generating demo pair files\n");
	char priv1[] = "";
	char priv2[] = "";
	char priv3[] = "";

	saveDogecoinAddress(demoPair1, demo_address1, priv1);
	saveDogecoinAddress(demoPair2, demo_address2, priv2);
	saveDogecoinAddress(demoPair3, demo_address3, priv3);
	printf("Files created!");
}

void demoSetupNodeAddress()
{
	printf("Setting up demo node addresses...\n");
	cmdSetLocalAddress(myaddr[0], myaddr[1], myaddr[2]);
}

/// <summary>
/// For testing loading of generated address pair files
/// </summary>
/// <param name="pairIndex"></param>
void loadDemoPairFileHelper(int pairIndex)
{
	char* filename;
	switch (pairIndex)
	{
	case 0:
		filename = demoPair1;
		break;
	case 1:
		filename = demoPair2;
		break;
	case 2:
		filename = demoPair3;
		break;
	}
	loadDogecoinAddress(filename, loadedDogeAddress, loadedPrivateKey);
	// @TODO debug and demo purposes only remove later
	printf("Loaded...\nAddress: %s\nPrivate Key: %s\n", loadedDogeAddress, loadedPrivateKey);
}

int main()
{
	printf("Performing startup functions...\n");
	// Start by attempting to setup serial communication
	USB = 0;     // File descriptor set to zero.
	openPort(); // Open the port

	init(); // Init the port parameters -- file descriptor (USB) will be set

	pollEnable = 1;
	pthread_t serThreadID;
	pthread_create(&serThreadID, NULL, serialPollThread, (void*)&serThreadID);
	printf("Connection to LoRa hardware successful!\n");

	printf("\nChecking for libdogecoin integration...\n\n", NULL);

	//start the libdogecoin elliptical crypto mem space
	dogecoin_ecc_start(); 

	//set up a buffer string the size of a dogecoin address (P2PKH address) - in include/constants.h
	char returnedaddr[P2PKH_ADDR_STRINGLEN];

	if (testLib(returnedaddr))
	{
		printf("Libdogecoin found.\n", NULL);
	    printf("Libdogecoin TEST - randomly generated test addr : % s \n\n", returnedaddr);
	}
	else
	{
		printf("Libdogecoin not responding or error. \n");
		exit(EXIT_FAILURE);
	}
	printf("Libdogecoin initialization complete!\n");

	printStartScreen();
	createTestDogeAddress(loadedDogeAddress, generatedPrivateKey); // Initial test address 
	if (demoMode)
	{
		demoSetupNodeAddress();
	}
	// Enter into mode selection loop
	modeSelectionLoop();

	printf("Exiting!\n");
	pollEnable = 0;
	do {
		sleep(1);
	} while (pthread_join(serThreadID, NULL));
	//Stop the libdogecoin ecc 
	dogecoin_ecc_stop();
	return 0;
}


