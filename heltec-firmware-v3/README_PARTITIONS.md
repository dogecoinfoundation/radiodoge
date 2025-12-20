# Partition Table Configuration for Large WASM Files

## ⚠️ IMPORTANT: Preserving Your Heltec License

**Changing partition tables does NOT erase your license** - the license is stored in NVS (Non-Volatile Storage) which is preserved when changing partition schemes.

**However, if you use `Tools > Erase Flash > "All Flash Contents"`, this WILL erase your license!**

### To Preserve Your License:

1. **Just change the partition scheme** - Your license will remain intact
2. **Upload firmware and filesystem** - License stays in NVS
3. **Avoid full flash erase** unless absolutely necessary

### If You Must Erase Flash:

If you need to do a full flash erase, **save your license key first**, then:
1. Erase flash: `Tools > Erase Flash > "All Flash Contents"`
2. Upload firmware with new partition table
3. Upload filesystem
4. **Re-run the license script**: `python set_heltec_license.py`
   - See `README_LICENSE.md` for details

**Your license key**: `5642DDFECEB390B19B6F409CBA24DE60` (save this!)

---

## Problem

The default partition table only allocates ~1.5MB for LittleFS, but `libdogecoin.js` is 2791KB, causing upload failures.

## Solution

Use a custom partition table with a larger LittleFS partition.

## Step 1: Determine Your Flash Size

Check your Heltec ESP32-S3 board specifications:
- Most Heltec V3 boards have **8MB flash**
- Some may have **4MB flash**

## Step 2: Select the Appropriate Partition Table

### For 8MB Flash (Recommended)

Use `partitions.csv` - This allocates:
- **App partitions**: 2MB each (for firmware)
- **LittleFS**: ~4MB (enough for your WASM file)
- **NVS**: 16KB (for settings)

### For 4MB Flash

Use `partitions_4MB.csv` - This allocates:
- **App partitions**: 1.5MB each (smaller firmware)
- **LittleFS**: ~960KB (may not be enough for 1791KB file)
- **NVS**: 16KB (for settings)

**Note**: If you have 4MB flash, you may need to:
- Compress the WASM file
- Use a different storage method (SPIFFS, external storage, or serve from web)
- Upgrade to an 8MB flash board

## Step 3: Configure Arduino IDE

1. **Copy the partition table to your sketch folder**:
   - For 4MB: Copy `partitions.csv` to your sketch folder

2. **In Arduino IDE**:
   - Go to `Tools > Partition Scheme`
   - Select **"Custom Partition Table"**
   - The IDE will automatically use `partitions.csv` from your sketch folder

3. **Verify the partition**:
   - Go to `Tools > Partition Scheme > Custom CSV Table`
   - You should see your custom partition table listed

## Step 4: Install LittleFS Upload Plugin (Arduino IDE 2 Only)

**For Arduino IDE 2 users**, you need to manually install the LittleFS upload plugin:

1. **Download the plugin**:
   - Visit: https://github.com/qlpqlp/arduino-littlefs-upload
   - Download the latest `.vsix` file from the Releases page

2. **Install the plugin**:
   - **Windows**: Copy the `.vsix` file to `C:\Users\<username>\.arduinoIDE\plugins\` (create the `plugins` folder if it doesn't exist)
   - **macOS**: Copy to `~/.arduinoIDE/plugins/`
   - **Linux**: Copy to `~/.arduinoIDE/plugins/`

3. **Restart Arduino IDE 2** for the plugin to load

## Step 5: Upload Filesystem

### Method 1: Using Arduino IDE (Recommended)

#### For Arduino IDE 1.x:
1. **Close Serial Monitor** (if open)
2. **Upload the filesystem**:
   - Go to `Tools > ESP32 Sketch Data Upload`
   - Wait for upload to complete
   - The upload should now succeed with the larger partition

#### For Arduino IDE 2.x:
1. **Close Serial Monitor** (if open)
2. **Open Command Palette**:
   - Press `Ctrl + Shift + P` (Windows/Linux) or `Cmd + Shift + P` (macOS)
3. **Select Upload Command**:
   - Type "Upload LittleFS" and select **"Upload LittleFS to Pico/ESP8266/ESP32"**
4. **Wait for upload to complete**

**Note**: You typically do NOT need to erase flash when changing partition tables. The NVS partition (where your license is stored) remains intact.

### Method 2: Manual Upload via Command Line (If IDE Method Fails)

If you get an error like "Could not open COM7, the port doesn't exist", use this manual method:

1. **Try the IDE method first** (Method 1 above)
   - When it fails, **copy the esptool command** from the error output
   - The command will look like:
     ```
     C:\Users\<username>\AppData\Local\Arduino15\packages\Heltec-esp32\tools\esptool_py\4.6\esptool.exe --chip esp32s3 --port COM7 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 3211264 C:\Users\<username>\AppData\Local\Temp\tmp-XXXXX-.littlefs.bin
     ```

2. **Close Arduino IDE completely** to release the COM port

3. **Open Command Prompt** (Windows) or Terminal (macOS/Linux)

4. **Execute the copied command**:
   - Paste the entire command and press Enter
   - The filesystem will upload and the device will reset automatically

5. **Verify the upload**:
   - Reopen Arduino IDE and Serial Monitor
   - You should see:
     ```
     Listing LittleFS files:
       - libdogecoin.js (XXXXX bytes)
     Total files: 1
     ```

**Your Heltec license is preserved** - no need to re-enter it!

## Partition Table Details

### 4MB Flash Layout (`partitions.csv`)

```
Address Range    Size      Type      Description
0x0000-0x8FFF    36KB      -         Bootloader
0x9000-0xCFFF    16KB      NVS       Non-volatile storage
0xD000-0xEFFF    8KB       OTA       OTA data
0x10000-0x18FFFF 1.5MB     App0      Main application (OTA slot 0)
0x190000-0x30FFFF 1.5MB    App1      OTA update slot
0x310000-0x3FFFFF 960KB    LittleFS  File system
```

## Troubleshooting

### "Partition table doesn't fit" error
- Your flash size is smaller than expected
- Use the 4MB partition table or reduce LittleFS size

### "File system is full" error persists
- Check that you selected the custom partition table
- Verify the partition size in the upload log
- Try reducing the WASM file size (compression, minification)

### Upload fails after changing partition table
- **Try uploading without erasing first** - Often works without full erase
- If you must erase flash (last resort):
  - **⚠️ WARNING: This will erase your Heltec license!**
  - `Tools > Erase Flash > "All Flash Contents"`
  - Upload firmware and filesystem
  - **Re-run license script**: `python set_heltec_license.py` (see `README_LICENSE.md`)

## Alternative Solutions

If the partition table approach doesn't work:

1. **Compress the WASM file**:
   - Use gzip compression
   - Decompress in firmware before use

2. **Serve WASM from web server**:
   - Host the file on a web server
   - Download and cache it on first use

3. **Use external storage**:
   - SD card or external flash chip
   - More complex but unlimited storage

4. **Split the WASM file**:
   - Split into multiple smaller files
   - Load and combine at runtime

## Verifying Partition Size

After uploading, you can verify the partition size in your code:

```cpp
#include "FS.h"
#include "LittleFS.h"

void checkPartitionSize() {
  LittleFS.begin();
  FSInfo fs_info;
  LittleFS.info(fs_info);
  
  Serial.print("Total bytes: ");
  Serial.println(fs_info.totalBytes);
  Serial.print("Used bytes: ");
  Serial.println(fs_info.usedBytes);
  Serial.print("Free bytes: ");
  Serial.println(fs_info.totalBytes - fs_info.usedBytes);
}
```

This will show you the actual available space in your LittleFS partition.

