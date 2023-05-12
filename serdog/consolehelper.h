#ifndef CONSOLEHELPER_H
#define CONSOLEHELPER_H

#include <stdio.h> 
#include <stdint.h>
#include <string.h>

void printStartScreen();
void printNodeAddress(char* nodeTitle, uint8_t* address);
int getModeSelection();
int getSetupModeSelection();
int getDogeModeSelection();
int getTestModeSelection();
int getDemoAddressSelection();
void getUserSuppliedNodeAddress(uint8_t* address);

#endif