#ifndef RADIO_DOGE_DISPLAY
#define RADIO_DOGE_DISPLAY

#include "HT_SSD1306Wire.h"
#include "radioDogeTypes.h"
#include "Images/logoImage.h"
#include "Images/coin.h"
#include "Images/doge.h"
#include "Images/sendingDogeCoin.h"
#include "Images/receivingDogeCoin.h"

// Animation and display related constants
#define ANIMATION_FRAME_DELAY 5
#define MIDDLE_OF_SCREEN 27  // Assumes text is 10 pixels in length so half would be (64/2) - (10/2) = 27
#define TEST_COIN_AMOUNT 1234.56

void HeltecDisplayInit();
// Display hardware information on the screen
void DisplayHardwareInfo(int boardVersion);
// Animates a coin icon going across the screen
void CoinAnimation();
// Animates a doge face going across the screen
void DogeAnimation();
// Draw static RadioDoge Logo
void DrawRadioDogeLogo();
// Draw sending coin image
void DrawSendingCoinsImage(float numCoins);
// Draw receiving coins image
void DrawReceivingCoinsImage(float numCoins);
// Display received command information from the host on the module's screen
void DisplayCommandAndControl(uint8_t commandVal);
// Display a transmitted message on the module's screen
void DisplayTXMessage(String toDisplay, nodeAddress destNode);
// Display a received message on the module's screen
void DisplayRXMessage(String rxMessage, nodeAddress sender);
// Display a received broadcast message on the module's screen
void DisplayBroadcastMessage(String toDisplay, nodeAddress sendingNode);
// Display a custom string on the OLED
void DisplayCustomStringMessage(String customMessage, int yOffset);
// Display the modules local address on the screen
void DisplayLocalAddress(nodeAddress localAddress);

#endif