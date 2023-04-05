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
void getUserSuppliedNodeAddress(uint8_t* address);

#endif