#include "consolehelper.h"

void printStartScreen()
{
	printf("\nWelcome to RadioDoge! Press ENTER to continue...");
	getchar();
}

int getModeSelection()
{
	printf("\n### MUCH MODE SELECT (0-3) ###\n");
	printf("1: LoRa Setup Mode\n");
	printf("2: Doge Mode\n");
	printf("3: Test Mode\n");
	printf("4: Quit\n");
	return userInputLoop(4);
}

int getSetupModeSelection()
{
	printf("\n### MUCH SETUP MODE SELECT ###\n");
	printf("1: Get Local Node Address\n");
	printf("2: Set Local Node Address\n");
	printf("3: Set Destination Node Address\n");
	printf("4: Send Ping\n");
	printf("5: Send Message\n");
	printf("6: Get Hardware Information\n");
	printf("7: Exit Setup Mode\n");
	return userInputLoop(7);
}

int getDogeModeSelection()
{
	printf("\n### MUCH DOGE MODE SELECT ###\n");
	printf("1: Get Dogecoin Address\n");
	printf("2: Get Dogecoin Balance\n");
	printf("3: Display QR code\n");
	printf("4: Exit Doge Mode\n");
	return userInputLoop(4);
}

int getTestModeSelection()
{
	printf("\n### MUCH TEST MODE SELECT ###\n");
	printf("1: Send Test Packet\n");
	printf("2: Multipart Packet Test\n");
	printf("3: Display Control Test\n");
	printf("4: Exit Test Mode\n");
	return userInputLoop(4);
}

void getUserSuppliedNodeAddress(uint8_t* address)
{
	printf("Enter in the node address (region.community.node):\n");
	char userString[64];
	scanf("%s", userString);
	// Extract the first token
	char* token = strtok(userString, ".");
	// loop through the string to extract all tokens
	int tokenCount = 0;
	while(token != NULL && tokenCount < 3)
	{
		address[tokenCount] = (uint8_t)atoi(token);
		token = strtok(NULL, ".");
		tokenCount++;
	}
}

void printNodeAddress(char* nodeTitle, uint8_t* address)
{
	printf("%s Node Address: %i.%i.%i\n", nodeTitle, address[0], address[1], address[2]);
}

int userInputLoop(int upperBound)
{
	printf("Enter a value between 1 and %i\n", upperBound);
	char userInput[128];
	int userSelection;
	while (1)
	{
		fgets(userInput, 128, stdin);
		userSelection = atoi(userInput);
		if (userSelection >= 1 && userSelection <= (upperBound - 1))
		{
			return userSelection;
		}
		else if (userSelection == upperBound)
		{
			// Exit value
			return -1;
		}
		else
		{
			printf("Invalid selection please enter a value between 1-%i\n", upperBound);
		}
	}
}