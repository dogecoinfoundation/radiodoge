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
int getManualUtxosEditingSelection();
void getUserSuppliedNodeAddress(uint8_t* address);
void getUserSuppliedDogecoinAmount(char* dogeAmount);
void getUserSuppliedPin(uint8_t* pin);

#endif