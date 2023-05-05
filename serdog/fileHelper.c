#include "fileHelper.h"

void encryptString(char* password, char* string, int stringLength) {
	int i, j = 0;
	int passwordLength = strlen(password);
	for (i = 0; i < stringLength; i++)
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
	encryptString(password, privateKey, WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN);
	fclose(file);
}

void writeAddressToFile(char* fileName, char* pubAddress, char* privateKey, char* password) {
	FILE* file = fopen(fileName, "w");
	if (file == NULL)
	{
		printf("Error opening file.\n");
		return;
	}
	encryptString(password, privateKey, WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN);
	fprintf(file, "%s", pubAddress, privateKey);
	fwrite(privateKey, sizeof(char), WIF_UNCOMPRESSED_PRIVKEY_STRINGLEN, file);
	fclose(file);
}