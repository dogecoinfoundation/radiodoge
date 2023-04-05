#include "consolehelper.h"

void printStartScreen()
{
	printf("\nWelcome to RadioDoge! Press ENTER to continue...");
	getchar();
}

int getModeSelection()
{
	printf("\n### MUCH MODE SELECT (0-3) ###\n");
	printf("0: LoRa Setup Mode\n");
	printf("1: Doge Mode\n");
	printf("2: Test Mode\n");
	printf("3: Quit\n");
	return userInputLoop(3);
}

int getSetupModeSelection()
{
	printf("\n### MUCH SETUP MODE SELECT ###\n");
	printf("0: Get Local Node Address\n");
	printf("1: Set Local Node Address\n");
	printf("2: Set Destination Node Address\n");
	printf("3: Send Ping\n");
	printf("4: Send Message\n");
	printf("5: Get Hardware Information\n");
	printf("6: Exit Setup Mode\n");
	return userInputLoop(6);
}

int getDogeModeSelection()
{
	printf("\n### MUCH DOGE MODE SELECT ###\n");
	printf("0: Get Dogecoin Address\n");
	printf("1: Get Dogecoin Balance\n");
	printf("2: Exit Doge Mode\n");
	return userInputLoop(2);
}

int getTestModeSelection()
{
	printf("\n### MUCH TEST MODE SELECT ###\n");
	printf("0: Send Test Packet\n");
	printf("1: Multipart Packet Test\n");
	printf("2: Exit Test Mode\n");
	return userInputLoop(2);
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

int userInputLoop(int upperBound)
{
	printf("Enter a value between 0 and %i\n", upperBound);
	int userSelection;
	while (1)
	{
		scanf("%i", &userSelection);
		if (userSelection >= 0 && userSelection <= (upperBound - 1))
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
			printf("Invalid selection please enter a value between 0-%i\n", upperBound);
		}
	}
}