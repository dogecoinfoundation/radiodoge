#include "radioDogeDisplay.h"

char displayBuf[64];
SSD1306Wire oledDisplay(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

void HeltecDisplayInit()
{
  oledDisplay.init();
  oledDisplay.setTextAlignment(TEXT_ALIGN_LEFT);
  oledDisplay.setFont(ArialMT_Plain_10);
}

// Display hardware information on the screen
void DisplayHardwareInfo(int boardVersion) {
  oledDisplay.clear();
  sprintf(displayBuf, "Heltec WiFi LoRa 32 V%d", boardVersion);
  oledDisplay.drawString(0, MIDDLE_OF_SCREEN, displayBuf);
  oledDisplay.display();
}

// Animates a coin icon going across the screen
void CoinAnimation() {
  int xPos = 0;
  while (true) {
    oledDisplay.clear();
    oledDisplay.drawXbm(xPos - 64, 0, coin_width, coin_height, coin_bits);
    oledDisplay.display();
    xPos++;
    if (xPos % 192 == 0) {
      oledDisplay.clear();
      return;
    }
    delay(ANIMATION_FRAME_DELAY);
  }
}

// Animates a doge face going across the screen
void DogeAnimation() {
  int xPos = 0;
  while (true) {
    oledDisplay.clear();
    oledDisplay.drawXbm(xPos - 64, 0, doge_width, doge_height, doge_bits);
    oledDisplay.display();
    xPos++;
    if (xPos % 192 == 0) {
      oledDisplay.clear();
      return;
    }
    delay(ANIMATION_FRAME_DELAY);
  }
}

// Draw static RadioDoge Logo
void DrawRadioDogeLogo() {
  oledDisplay.clear();
  oledDisplay.drawXbm(0, 0, logo_width, logo_height, logo_bits);
  oledDisplay.display();
}

// Draw sending coin image
void DrawSendingCoinsImage(float numCoins) {
  oledDisplay.clear();
  oledDisplay.drawXbm(0, 0, sendCoin_width, sendCoin_height, sendCoin_bits);
  sprintf(displayBuf, "%0.2f", numCoins);
  oledDisplay.drawString(5, MIDDLE_OF_SCREEN, displayBuf);
  oledDisplay.display();
}

// Draw receiving coins image
void DrawReceivingCoinsImage(float numCoins) {
  oledDisplay.clear();
  oledDisplay.drawXbm(0, 0, rcvCoin_width, rcvCoin_height, rcvCoin_bits);
  sprintf(displayBuf, "%0.2f", numCoins);
  oledDisplay.drawString(5, MIDDLE_OF_SCREEN, displayBuf);
  oledDisplay.display();
}

// Display received command information from the host on the module's screen
void DisplayCommandAndControl(uint8_t commandVal) {
  oledDisplay.clear();
  sprintf(displayBuf, "CMD FR HOST: %d", commandVal);
  oledDisplay.drawString(0, 5, displayBuf);
  String commandTypeString;
  switch (commandVal) {
    case ADDRESS_GET:
      commandTypeString = "Get Address";
      break;
    case ADDRESS_SET:
      commandTypeString = "Set Address";
      break;
    case PING_REQUEST:
      commandTypeString = "Send Ping";
      break;
    case MESSAGE_REQUEST:
      commandTypeString = "Send Message";
      break;
    case HARDWARE_INFO:
      commandTypeString = "Get Hardware Info";
      break;
    case BROADCAST_MESSAGE:
      commandTypeString = "Broadcast Message";
      break;
    case DISPLAY_CONTROL:
      commandTypeString = "Display Control";
      break;
    case HOST_FORMED_PACKET:
      commandTypeString = "Host formed packet";
      break;
    case MULTIPART_PACKET:
      commandTypeString = "Multipart packet";
      break;
    default:
      commandTypeString = "Wat!?";
      break;
  }
  oledDisplay.drawString(0, MIDDLE_OF_SCREEN, commandTypeString);
  oledDisplay.display();
}

// Display a transmitted message on the module's screen
void DisplayTXMessage(String toDisplay, nodeAddress destNode) {
  oledDisplay.clear();
  sprintf(displayBuf, "Sending to %d.%d.%d:", destNode.region, destNode.community, destNode.node);
  oledDisplay.drawString(0, 5, displayBuf);
  oledDisplay.drawString(0, MIDDLE_OF_SCREEN, toDisplay);
  oledDisplay.display();
}

// Display a received broadcast message on the module's screen
void DisplayBroadcastMessage(String toDisplay, nodeAddress sendingNode)
{
  oledDisplay.clear();
  sprintf(displayBuf, "Broadcast from %d.%d.%d:", sendingNode.region, sendingNode.community, sendingNode.node);
  oledDisplay.drawString(0, 5, displayBuf);
  oledDisplay.drawString(0, MIDDLE_OF_SCREEN, toDisplay);
  oledDisplay.display();
}

// Display a received message on the module's screen
void DisplayRXMessage(String rxMessage, nodeAddress sender) {
  oledDisplay.clear();
  sprintf(displayBuf, "Received from %d.%d.%d:", sender.region, sender.community, sender.node);
  oledDisplay.drawString(0, 5, displayBuf);
  oledDisplay.drawString(0, MIDDLE_OF_SCREEN, rxMessage);
  oledDisplay.display();
}

// Display a custom string on the OLED
void DisplayCustomStringMessage(String customMessage, int yOffset)
{
  oledDisplay.clear();
  oledDisplay.drawString(0, yOffset, customMessage);
  oledDisplay.display();
}

// Display the modules local address on the screen
void DisplayLocalAddress(nodeAddress localAddress) {
  oledDisplay.clear();
  sprintf(displayBuf, "Hi I'm %d.%d.%d", localAddress.region, localAddress.community, localAddress.node);
  oledDisplay.drawString(0, MIDDLE_OF_SCREEN, displayBuf);
  oledDisplay.display();
}