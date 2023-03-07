// serialDoge 0.01 serialcomms for RadioDoge
//

#include "serdog.h"
#include <stdint.h>


int openport()
{
	printf("Trying to open %s...\n",device);
	USB = open(device, O_RDWR | O_NOCTTY);
	if (USB == -1)
	{
		fprintf(stderr, "Unable to open %s: error= %s\n\n",device, strerror(errno));
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("Open returned file descriptor [%i] with port open status [%s]\n\n", USB,strerror(errno));
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
	tty.c_cflag &= ~PARENB;             // No parity bit
	tty.c_cflag &= ~CSTOPB;             // 1 stop bit
	tty.c_cflag &= ~CSIZE;              // Mask data size
	tty.c_cflag |= CS8;                // Select 8 data bits

	tty.c_cflag &= ~CRTSCTS;           // no flow control
	tty.c_cc[VMIN] = 1;                  // read doesn't block
	tty.c_cc[VTIME] = 5;                  // 0.5 seconds read timeout

	tty.c_cflag |= (CLOCAL | CREAD);    // turn on READ & ignore ctrl lines

	// Enable data to be processed as raw input
	tty.c_lflag &= ~(ICANON | ECHO | ISIG);


	printf("terminal parameters set. \n");

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

int sendCommand(enum serialCommand cmdtype, uint8_t* payload, int payloadsize, char* returnedbuffer)
{

	int n_written = 0;
	int total_written = 0;
    int idx = 0;
	uint8_t currCommand = 0;
	size_t cmdlength = 0;
	uint8_t payloadlen = 0;
	uint8_t txbytes[255+2]; //payload+hdrlen (cmd+payloadlenbyte)
	int intcmd = cmdtype;
	if (payload !=NULL)
	{
		printf("\ntest\n");
		payloadlen = strlen(payload);
	}


	//sprintf(txbytes,"%x", cmdtype); //easy way of converting an into to a character in a string.

	printf("\nincoming command type %d\n", cmdtype);

	txbytes[0] = (uint8_t)cmdtype;
	txbytes[1] = (uint8_t)payloadlen;

	printf("cmd type is [%02X]\n", txbytes[0]);
	printf("embed payload len is [%02X]\n", txbytes[1]);
	if (payload != 0)
	{
		printf("Payload length %i\n", payloadlen);
		printf("Payload is :");
		idx = 0;
		do
		{
			printf(" [%02X] ", payload[idx]);
			txbytes[idx + 2] = payload[idx];//+2 because commandtype and payloadlen has been added
			idx++;
		} while (idx != payloadlen);
	}


	printf("\n");

	//printf("terminator is [%02X] or char %c\n\n", EOTX,EOTX);

	//bytecat(txbytes, EOTX);
	
	

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
	printf("Writing bytes: ");
	do {

		printf("[%02X]", txbytes[idx]);
		
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
	} 
	while (total_written != (payloadlen+hdrlen) && n_written > 0);
	printf(" to port [%s]\n", device);

	printf("[%i] total bytes written to [%s]\n", total_written, device);


}

int cmdSetLocalAddress(int region, int community, int node, uint8_t* rxbuf)
{
	int cmdtype = ADDRESS_SET; //we're working with address set for all of this

	uint8_t payload[3] = { (uint8_t)region,(uint8_t)community,(uint8_t)node};

	sendCommand(cmdtype, payload,3, rxbuf);

	uint8_t respCmdType = 0;
	size_t resplen = 0;

	parsePortResponse(respCmdType,resplen,rxbuf);
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

int cmdGetLocalAddress(char* rxbuf)
{
	//no payload = 0.
	uint8_t cmdtype = ADDRESS_GET;

	sendCommand(cmdtype,0,0,rxbuf);
	uint8_t respCmdType = 0;
	size_t resplen = 0;

	parsePortResponse(respCmdType, resplen, rxbuf);

};

int cmdSendPingCmd(uint8_t* inAddr, char* rxbuf)
{
	uint8_t cmdtype = PING_REQUEST;
	int payloadsize = 3;
	uint8_t payload[3] = {inAddr[0],inAddr[1],inAddr[2]};

	sendCommand(cmdtype,payload,payloadsize,rxbuf);

	int rxlin = 10;
	char temprxbuf[1024];
	//one line not 5 later.
	printf("Read:\n");

	for (int rx = 1; rx <= rxlin; rx++)
	{
		memset(temprxbuf, 0, sizeof temprxbuf);

		uint8_t respCmdType = 0;
		uint8_t resplen = 0;

		parsePortResponse(respCmdType, resplen, rxbuf);
		printf("[Line %i, %li bytes]:%s\n", rx, strlen(rxbuf), temprxbuf);
		strcat(rxbuf, "\n");
		strcat(rxbuf, temprxbuf);
	}
	uint8_t edas_this[] = {0x46, 0x4F, 0x52, 0x20, 0x4D, 0x45};// FOR ME Message follows: MSGTYPE, FR, ADDR

	char* substr = strstr(rxbuf, "FOR ME");

	printf("Found %s\n",substr);

};

void printByteArray(uint8_t* array)
{
	for (int nx = 0; nx < sizeof(array); nx++)
	{
		//
	}
}

int main()
{
	USB = 0;     // File descriptor set to zero.
	char rxbuf[1024];//receive buffer for responses.
	int resplen = 0;
	printf("SerDog starting...\n\n");

	openport();

	init();

	//setLocalAddr
	printf("CMD TEST: Setting local address to %i.%i.%i.\n", myaddr[0], myaddr[1], myaddr[2]);
	cmdSetLocalAddress(myaddr[0], myaddr[1], myaddr[2],rxbuf);

	printf("\nFrom command, received: \n\"[%02X]\"\n\n", rxbuf[0]);

	//queryLocalAddr
	printf("CMD TEST: Querying Local Address...\n");
	cmdGetLocalAddress(rxbuf);
	printf("\nFrom command, received: \n[%02X][%02X][%02X]\n\n", rxbuf[0],rxbuf[1],rxbuf[2]);

	//pingAnother
	printf("CMD TEST: Pinging remote %i.%i.%i.\n", rmaddr[0], rmaddr[1], rmaddr[2]);
	cmdSendPingCmd(rmaddr, rxbuf);
	printf("\nFrom command, received: \n[%s]\n\n", rxbuf);

	return 0;
}


