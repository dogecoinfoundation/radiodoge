#include "fileHelper.h"

void encryptString(char* password, char* string, int stringLength, int startingIndex) {
	int i, j = 0;
	int passwordLength = strlen(password);
	for (i = startingIndex; i < stringLength; i++)
	{
		string[i] ^= password[j];
		j = (j + 1) % passwordLength;
	}
}

void readAddressFromFile(char* fileName, char* pubAddress, char* privateKey, char* password) {
	FILE* file = fopen(fileName, "r");
	if (file == NULL)
	{
		printf("Error opening file.\n");
		exit(1);
	}
	fgets(pubAddress, P2PKH_ADDR_STRINGLEN, file);
	fread(privateKey, sizeof(char), WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN, file);
	// Don't want to encrypt/decrypt the Q as it could then be easier to figure out password
	encryptString(password, privateKey, WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN, 1);
	fclose(file);
}

void writeAddressToFile(char* fileName, char* pubAddress, char* privateKey, char* password) {
	FILE* file = fopen(fileName, "w");
	if (file == NULL)
	{
		printf("Error opening file.\n");
		return;
	}
	// Don't want to encrypt/decrypt the Q as it could then be easier to figure out password
	encryptString(password, privateKey, WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN, 1);
	fprintf(file, "%s", pubAddress, privateKey);
	fwrite(privateKey, sizeof(char), WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN, file);
	fclose(file);
}

void getUserPassword(char* userInputBuffer, bool isLoad)
{
	if (isLoad)
	{
		printf("Please enter the password used to encrypt the file:\n");
	}
	else
	{
		printf("Please enter a password to encrypt the private key:\n");
	}
	scanf("%s", userInputBuffer);
}

void saveDogecoinAddress(char* filename, char* pubAddress, char* privateKey)
{
	char passwordBuffer[32];
	getUserPassword(passwordBuffer, false);
	writeAddressToFile(filename, pubAddress, privateKey, passwordBuffer);
}

void loadDogecoinAddress(char* filename, char* pubAddress, char* privateKey)
{
	char passwordBuffer[32];
	getUserPassword(passwordBuffer, true);
	readAddressFromFile(filename, pubAddress, privateKey, passwordBuffer);
}