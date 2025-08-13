#ifndef CONSOLEHELPER_H
#define CONSOLEHELPER_H

#include <stdio.h> 
#include <stdint.h>
#include <string.h>

void printStartScreen();
int getModeSelection();
int getSetupModeSelection();
int getDogeModeSelection();
int getTestModeSelection();
int getDemoAddressSelection();
int getManualUtxosEditingSelection();
int getKeyEditingSelection();
void getUserSuppliedNodeAddress(uint8_t* address);
void getUserSuppliedPin(uint8_t* pin);
void getUserSuppliedDogecoinAmount(char* dogeAmount);
void printNodeAddress(char* nodeTitle, uint8_t* address);
int userInputLoop(int upperBound);
void printByteArray(uint8_t* arrayIn, int length);

#endif
