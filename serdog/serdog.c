// serialDoge 0.01 serialcomms for RadioDoge
//

#include "serdog.h"
#include <sys/poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <pthread.h>

int pollEnable = 0;

int charsinbuffer = 0;

int openport()
{
	printf("Trying to open %s...\n",device);
	USB = open(device, O_RDWR | O_NOCTTY ); //should be nonblock for polling
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

int sendCommand(enum serialCommand cmdtype, int payloadsize, uint8_t* payload)
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

int cmdSetLocalAddress(int region, int community, int node)
{
	int cmdtype = ADDRESS_SET; //we're working with address set for all of this

	uint8_t payload[3] = { (uint8_t)region,(uint8_t)community,(uint8_t)node};

	sendCommand(cmdtype,3,payload);
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

int cmdGetLocalAddress()
{
	//no payload = 0.
	uint8_t cmdtype = ADDRESS_GET;

	sendCommand(cmdtype,0,0);

};

int cmdSendPingCmd(uint8_t* inAddr)
{
	uint8_t cmdtype = PING_REQUEST;
	int payloadsize = 3;
	uint8_t payload[3] = {inAddr[0],inAddr[1],inAddr[2]};

	sendCommand(cmdtype,payloadsize,payload);

};

void printByteArray(uint8_t* array)
{
	for (int nx = 0; nx < sizeof(array); nx++)
	{
		printf("[%02X]", array[nx]);
	}
}


void *serialPollThread(void* threadid)
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



	do
	{
		int serReceivedCharacters = poll(serFileDescriptor, 1, 1000);

		if (serReceivedCharacters < 0)
		{
			//perror("poll error");
			printf("*** THREAD: error in poll thread \n",NULL);
		}
		else if (serReceivedCharacters > 0)
		{
			uint8_t minibuffer[255] = { 0 };
			if (serFileDescriptor[0].revents & POLLIN)
			{
				
				ssize_t receivedCharactersLen = read(USB, minibuffer, sizeof(minibuffer));
				if (receivedCharactersLen > 0)
				{
					printf("*** THREAD: Received %i chars\n", receivedCharactersLen);
					
					
					for (int rc = 0; rc < receivedCharactersLen; rc++)
					{
						
						printf("[%02x]", (uint8_t)minibuffer[rc]);
						rxbuffer[totalRxChars] = minibuffer[rc];
						totalRxChars++;
					}
					
					printf("\n",NULL);
				}
			}
		}

		rxPayloadSize = isCompleteCmd(rxbuffer, totalRxChars);
		if (rxPayloadSize>=0)
		{
			
			printf("**Valid and complete cmd** totRX=%i psize=%i\n", totalRxChars, rxPayloadSize);
		}
		else
		{
			printf("***Not cmd** totRX=%i\n", totalRxChars);
		}
	} while (pollEnable == 1);
	printf("*** THREAD: Exiting thread %d ..\n", thisid);
	pthread_exit(0);
}

int isCompleteCmd(uint8_t* inBuf,int charsReceived)
{
	if (charsReceived > 2)
	{
		printf("int inbuf %i %i %i\n", (int)inBuf[0],(int)inBuf[1],(int)inBuf[2]);
		if (isCmd(inBuf[0]) && (int)inBuf[1] == charsReceived - 2)
		{
			printf("charsReceived: %i\n", charsReceived);
			return ((int)inBuf[1]);
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
	case ADDRESS_GET:
		return (int)ADDRESS_GET;
		break;
	case ADDRESS_SET:
		return (int)ADDRESS_SET;
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
	case RESULT_CODE:
		return (int)RESULT_CODE;
		break;

	default: 
		return 0;
	}
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

main()
{
	USB = 0;     // File descriptor set to zero.
	printf("SerDog starting...\n\n",NULL);

	openport(); //open the port

	init(); //init the port paramters -- file descriptor (USB) will be set

	pollEnable = 1;
	pthread_t serThreadID;
	pthread_create(&serThreadID, NULL, serialPollThread, (void*)&serThreadID);

	//setLocalAddr
	printf("CMD TEST: Setting local address to %i.%i.%i.\n", myaddr[0], myaddr[1], myaddr[2]);
	cmdSetLocalAddress(myaddr[0], myaddr[1], myaddr[2]);
	waitForResponse(ADDRESS_SET);
	

	/*
	//queryLocalAddr
	printf("CMD TEST: Querying Local Address...\n");
	cmdGetLocalAddress(rxbuf);
	printf("\nFrom command, received: \n[%02X][%02X][%02X]\n\n", rxbuf[0],rxbuf[1],rxbuf[2]);

	
	//pingAnother
	printf("CMD TEST: Pinging remote %i.%i.%i.\n", rmaddr[0], rmaddr[1], rmaddr[2]);
	cmdSendPingCmd(rmaddr, rxbuf);
	printf("\nFrom command, received: \n[%s]\n\n", rxbuf);

	*/
	getc(stdin);//wait for keypress
	pollEnable = 0;
	do {
		sleep(1);
	} while (pthread_join(serThreadID,NULL));
	return 0;
}


