# Heltec License Setup (In Case You Brick or Erase Accidentally) 

## Quick Setup

After resetting the partition table, the Heltec license stored in NVS is erased. Use this script to restore it.

## Installation

1. Install Python 3.6 or higher
2. Install required library:
   ```bash
   pip install pyserial
   ```
   Or use the requirements file:
   ```bash
   pip install -r requirements.txt
   ```

## Usage

### Basic Usage (COM7)
```bash
python set_heltec_license.py
```

### Custom Port
```bash
python set_heltec_license.py COM3
```

### Custom Port and License Key
```bash
python set_heltec_license.py COM3 1234567890ABCDEFGHIJKLMN
```

## Steps

1. **Close Arduino IDE Serial Monitor** (if open)
2. **Connect your Heltec ESP32** to the computer
3. **Find your COM port**:
   - Windows: Device Manager > Ports (COM & LPT)
   - Linux: `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`
   - Mac: `ls /dev/tty.usbserial*` or `ls /dev/tty.usbmodem*`
4. **Edit the script** if your port is not COM3 (change `SERIAL_PORT = "COM3"` on line 14)
5. **Run the script**:
   ```bash
   python set_heltec_license.py
   ```
6. **Wait for "OK" response**
7. **Reset your device** (unplug/replug USB or press RESET button)
8. **Verify** - the license error should be gone

## Troubleshooting

### "Serial port error" or "Access denied"
- Close Arduino IDE Serial Monitor
- Close any other programs using the COM port
- Try running as administrator (Windows)

### "No response" or timeout
- Check that the device is powered on
- Verify the COM port number is correct
- Try a different USB cable
- Make sure baud rate is 115200

### License still shows error after running script
- Make sure you reset the device after running the script
- Try running the script again
- Check serial monitor output for any error messages

## Manual Method (Alternative)

If the script doesn't work, you can set the license manually:

1. Open Arduino IDE Serial Monitor
2. Set baud rate to 115200
3. Set line ending to "Both NL & CR" or "Newline"
4. Type: `AT+CDKEY=1234567890ABCDEFGHIJKLMN`
5. Press Send
6. Wait for "OK"
7. Reset device

## How to Get Your License Key

If you don't have your license key yet, follow these steps:

### Step 1: Find Your Chip ID Using Arduino IDE

1. **Connect your Heltec ESP32** to your computer via USB
2. **Open Arduino IDE**
3. **Select the correct board and port**:
   - Go to `Tools > Board > ESP32 Arduino > Heltec WiFi LoRa 32 (V3)` (or your board version)
   - Go to `Tools > Port` and select your COM port
4. **Open Serial Monitor**:
   - Go to `Tools > Serial Monitor` or press `Ctrl+Shift+M`
   - Set baud rate to **115200**
   - Set line ending to **"Both NL & CR"** or **"Newline"**
5. **Upload and run your firmware** (or any Heltec example code)
6. **Look for the Chip ID** in the Serial Monitor output
   - You should see a line like: `ESP32ChipID=D0CBA19E139C`
   - The Chip ID is the **12-character hex string** after `ESP32ChipID=`
   - Example: `D0CBA19E139C`

**Alternative Method - Simple Code to Get Chip ID:**

If your firmware doesn't display the Chip ID, you can upload this simple code to get it:

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  uint64_t chipid = ESP.getEfuseMac();
  Serial.printf("ESP32ChipID=%04X%08X\n", (uint16_t)(chipid>>32), (uint32_t)chipid);
}

void loop() {
  delay(1000);
}
```

Upload this code and check the Serial Monitor for the Chip ID.

### Step 2: Get License from Heltec Website

1. **Visit the Heltec License Search Page**:
   - Go to: https://resource.heltec.cn/search
   
2. **Enter Your Chip ID**:
   - In the search field labeled "Please enter product id："
   - Enter your **12-character Chip ID** (e.g., `D0CBA19E139C`)
   - Click "Confirm" or press Enter

3. **Retrieve Your License**:
   - The page will display your board information
   - Click on **"Relevant Resource"** to access the license key
   - Your license will be displayed as 4 hex values:
     - Format: `{0xXXXXXXXX, 0xXXXXXXXX, 0xXXXXXXXX, 0xXXXXXXXX}`
     - Example: `{0x5642DDFE, 0xCEB390B1, 0x9B6F409C, 0xBA24DE60}`

4. **Convert to AT Command Format**:
   - Remove the `0x` prefixes and commas
   - Concatenate all 4 values together
   - Example: `5642DDFECEB390B19B6F409CBA24DE60`
   - This is the license key you'll use with the script or AT command

### Step 3: Set the License

Once you have your license key, use one of these methods:

- **Python Script** (Recommended): Use `set_heltec_license.py` with your license key
- **AT Command**: Send `AT+CDKEY=YOUR_LICENSE_KEY` via Serial Monitor

### Troubleshooting License Retrieval

- **Can't find license on website**: Some boards may not have licenses in the database. Contact Heltec support at support@heltec.cn with proof of purchase to get a license regenerated.
- **Chip ID not showing**: Make sure your device is properly connected and the Serial Monitor is set to 115200 baud.

## License Key

Current license key for this device:
```
1234567890ABCDEFGHIJKLMN
```

This corresponds to the hex values:
- 0x123456
- 0x7890AB
- 0xCDEFGH
- 0xIJKLMN

