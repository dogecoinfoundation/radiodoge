#ifndef FILEHELPER_H
#define FILEHELPER_H

#include <stdio.h> 
#include <stdint.h>
#include <string.h>
#include "libdogecoin.h"

void encryptString(char* password, char* string, int stringLength);
void readAddressFromFile(char* fileName, char* pubAddress, char* privateKey, char* password);
void writeAddressToFile(char* fileName, char* pubAddress, char* privateKey, char* password);

#endif