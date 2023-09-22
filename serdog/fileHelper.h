#ifndef FILEHELPER_H
#define FILEHELPER_H

#include <stdio.h> 
#include <stdint.h>
#include <string.h>
#include "libdogecoin.h"

void saveDogecoinAddress(char* filename, char* pubAddress, char* privateKey);
void loadDogecoinAddress(char* filename, char* pubAddress, char* privateKey);

#endif