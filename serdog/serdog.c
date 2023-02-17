// serialDoge 0.01 serialcomms for RadioDoge
//

#include "serdog.h"

int USB = 0;
char* device = "/dev/ttyUSB0"; //autodetect this later via a known response string on the serport
struct termios tty;
struct termios tty_old;

uint8_t myaddr[] = { 0x0A, 0x05, 0x03 };
uint8_t rmaddr[] = { 0x0A, 0x05, 0x01 };

uint8_t EOTX = {0xFF};

//serial commands

enum SerialCommandType
{
	None,
	GetAddress,
	SetAddresses,
	Ping,
	Message
};


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

int sendCommand(enum SerialCommandType cmdtype, uint8_t* payload, char* returnedbuffer)
{
	int n_written = 0;
	int total_written = 0;
    int idx = 0;
	uint8_t currCommand = 0;
	size_t cmdlength = 0;
	uint8_t txbytes[255];
	int intcmd = cmdtype;


	//sprintf(txbytes,"%x", cmdtype); //easy way of converting an into to a character in a string.

	printf("\nincoming command type %d\n", cmdtype);

	txbytes[0] = (uint8_t)cmdtype;

	printf("cmd type is [%02X]\n", txbytes[0]);
	if (payload != 0)
	{
		size_t payloadlen = strlen(payload - 1);
		printf("Payload is :");
		idx = 0;
		do
		{
			printf(" [%02X] ", payload[idx]);
			txbytes[idx + 1] = payload[idx];//+1 because commandtype has been added
			idx++;
		} while (payload[idx - 1] != EOTX);
	}


	printf("\n");

	printf("terminator is [%02X] or char %c\n\n", EOTX,EOTX);

	bytecat(txbytes, EOTX);
	
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
	while (txbytes[idx-1] != EOTX && n_written > 0);
	printf(" to port [%s]\n", device);

	printf("[%i] total bytes written to [%s]\n", total_written, device);


}

int cmdSetLocalAddress(int region, int community, int node, char* rxbuf)
{

	uint8_t payload[4] = { (uint8_t)region,(uint8_t)community,(uint8_t)node,EOTX };

	sendCommand(SetAddresses, payload, rxbuf);

	readPortResponse(rxbuf);
}

int readPortResponse(char* rxbuf)
{
	int n_read = 0;
	int idx = 0;
	char buf = '\0';

	/* Whole response*/
	memset(rxbuf, '\0', sizeof rxbuf);
	do {
		n_read = read(USB, &buf, 1);
		if (buf != '\n')
		{
			sprintf(&rxbuf[idx], "%c", buf);
		};
		idx += n_read;
	} while (buf != '\n' && n_read > 0);



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
		return sizeof rxbuf;
	}

};

int cmdGetLocalAddress(char* rxbuf)
{
	//no payload = 0.

	sendCommand(GetAddress, 0, rxbuf);

	//hopefully this is one line later.

	printf("Read:");
	readPortResponse(rxbuf);
	printf("[Line 1, %i bytes]", strlen(rxbuf));
	char secondbuf[1024];
	readPortResponse(secondbuf);
	printf("[Line 2, %i bytes]", strlen(secondbuf));
	char thirdbuf[1024];
	readPortResponse(thirdbuf);
	printf("[Line 3, %i bytes]", strlen(thirdbuf));
	char fourthbuf[1024];
	readPortResponse(fourthbuf);
	printf("[Line 4, %i bytes]\n", strlen(fourthbuf));

	strcat(rxbuf, "\n");
	strcat(rxbuf, secondbuf);
	strcat(rxbuf, "\n");
	strcat(rxbuf, thirdbuf);
	strcat(rxbuf, "\n");
	strcat(rxbuf, fourthbuf);
};

int cmdSendPingCmd(uint8_t* inAddr, char* rxbuf)
{

	uint8_t payload[4] = { inAddr[0],inAddr[1],inAddr[2],EOTX };

	sendCommand(Ping, payload, rxbuf);

	//one line not 5 later.
	printf("Read:");
	readPortResponse(rxbuf);
	printf("[Line 1, %i bytes]", strlen(rxbuf));
	char secondbuf[1024];
	readPortResponse(secondbuf);
	printf("[Line 2, %i bytes]", strlen(secondbuf));
	char thirdbuf[1024];
	readPortResponse(thirdbuf);
	printf("[Line 3, %i bytes]", strlen(thirdbuf));
	char fourthbuf[1024];
	readPortResponse(fourthbuf);
	printf("[Line 4, %i bytes]", strlen(fourthbuf));
	char fifthbuf[1024];
	readPortResponse(fifthbuf);
	printf("[Line 5, %i bytes]\n", strlen(fifthbuf));

	strcat(rxbuf, "\n");
	strcat(rxbuf, secondbuf);
	strcat(rxbuf, "\n");
	strcat(rxbuf, thirdbuf);
	strcat(rxbuf, "\n");
	strcat(rxbuf, fourthbuf);
	strcat(rxbuf, "\n");
	strcat(rxbuf, fifthbuf);


};


int main()
{
	USB = 0;     // File descriptor set to zero.
	char rxbuf[1024];//receive buffer for responses.

	printf("SerDog starting...\n\n");

	openport();

	init();

	//setLocalAddr
	printf("CMD TEST: Setting local address to %i.%i.%i.\n", myaddr[0], myaddr[1], myaddr[2]);
	cmdSetLocalAddress(myaddr[0], myaddr[1], myaddr[2],rxbuf);

	printf("\nFrom command, received: \n\"%s\"\n\n", rxbuf);

	//queryLocalAddr
	printf("CMD TEST: Querying Local Address...\n");
	cmdGetLocalAddress(rxbuf);
	printf("\nFrom command, received: \n\"%s\"\n\n", rxbuf);

	//pingAnother
	printf("CMD TEST: Pinging remote %i.%i.%i.\n", rmaddr[0], rmaddr[1], rmaddr[2]);
	cmdSendPingCmd(rmaddr, rxbuf);
	printf("\nFrom command, received: \n\"%s\"\n\n", rxbuf);

	return 0;
}
