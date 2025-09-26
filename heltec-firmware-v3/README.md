# RadioDoge Heltec LoRa V3 Firmware

A comprehensive firmware for the Heltec WiFi LoRa 32 V3 module that enables secure Dogecoin transaction transmission via LoRa radio with dual WiFi connectivity, persistent configuration, and web-based management interface.

## 🚀 Features

### Core Functionality
- **LoRa Mesh Network**: P2P communication between RadioDoge devices
- **Dogecoin Transaction Support**: Send/receive signed Dogecoin transactions
- **Dual WiFi Mode**: Access Point + Station mode for internet connectivity
- **Web Interface**: Modern, responsive web UI for device management
- **REST API**: Complete API for programmatic control
- **Persistent Configuration**: All settings stored in NVS flash memory

### Security Features
- **Custom AP Password**: Secure, configurable WiFi access point password
- **Password Validation**: Strong password requirements (8-32 chars, letters + numbers)
- **Persistent Storage**: All configurations survive reboots
- **Secure API**: Protected web interface access
- **Password Management**: Change/reset AP password via web interface
- **Credential Storage**: Secure storage of WiFi credentials in NVS

### Network Features
- **Internet Gateway**: Forward transactions to Dogecoin network
- **Custom Gateways**: Support for custom transaction endpoints
- **Auto-Reconnection**: Automatic WiFi reconnection on boot
- **Dual Mode**: Simultaneous AP and internet connectivity
- **WiFi Credential Storage**: Persistent storage of internet WiFi credentials
- **Connection Status**: Real-time internet connectivity monitoring

### Device Management
- **Address Management**: Configurable LoRa network address (Region.Community.Node)
- **Status Monitoring**: Real-time device status and connection info
- **Configuration Reset**: Easy reset to default settings
- **Display Support**: OLED display with status information
- **LoRa Configuration Storage**: Persistent storage of LoRa network settings
- **Auto-Configuration**: Automatic restoration of last known settings on boot

## 📋 Hardware Requirements

- **Heltec WiFi LoRa 32 V3** module
- **USB Cable** for programming and power
- **Computer** with Arduino IDE installed

## 🛠 Installation & Setup

### 1. Install Arduino IDE
Download and install Arduino IDE from: https://www.arduino.cc/en/software

### 2. Install ESP32 Board Package
1. Open Arduino IDE
2. Go to **File > Preferences**
3. Add this URL to **Additional Board Manager URLs**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to **Tools > Board > Boards Manager**
5. Search for "ESP32" and install **ESP32 by Espressif Systems**

### 3. Install Required Libraries
Install these libraries via **Tools > Manage Libraries**:
- **Adafruit GFX Library** (by Adafruit)
- **Adafruit SSD1306** (by Adafruit)
- **LoRaWan_APP** (by Heltec)

### 4. Install USB Drivers (Windows)
Download and install USB drivers from:
https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

### 5. Configure Arduino IDE
1. Select **Tools > Board > ESP32 Arduino > Heltec WiFi LoRa 32 (V3)**
2. Select **Tools > Port > [Your COM Port]**
3. Set **Tools > Upload Speed > 921600**

## 🔧 Compilation & Upload

### 1. Open the Firmware
1. Open `heltec-firmware.ino` in Arduino IDE
2. Wait for all tabs to load (radioDogeTypes.h, Images/*.h)

### 2. Compile
- Click **Verify** (✓) to compile the code
- Fix any compilation errors if they appear

### 3. Upload to Device
1. Connect Heltec V3 to computer via USB
2. Press and hold **BOOT** button on device
3. Press and release **RESET** button while holding BOOT
4. Release **BOOT** button
5. Click **Upload** (→) in Arduino IDE
6. Wait for upload to complete

### 4. First Boot
1. Device will start with default settings
2. Look for "RadioDoge" WiFi network
3. Connect using password: `radiodoge`
4. Open browser to: `http://192.168.4.1`

## 📱 Web Interface Usage

### Initial Setup
1. **Connect to RadioDoge WiFi** (password: `radiodoge`)
2. **Open Web Interface**: `http://192.168.4.1`
3. **Configure Device**:
   - Set LoRa address in "Device Configuration"
   - Set internet WiFi in "WiFi Configuration"
   - Change AP password in "Access Point Password"

### Main Features

#### 🏠 Device Status
- Shows current LoRa address
- Displays WiFi and internet connection status
- Shows password security status

#### 📡 LoRa Communication
- **Send Messages**: Text messages to specific devices
- **Send Transactions**: Dogecoin transactions via LoRa
- **Broadcast**: Messages to all devices in range
- **Ping Test**: Test connectivity between devices
- **ACK**: Acknowledgment messages

#### 🌐 Internet Gateway
- **Transaction Forwarding**: Send transactions to Dogecoin network
- **Custom Gateways**: Use your own transaction endpoints
- **Auto-Forwarding**: Transactions automatically forwarded to internet

#### ⚙️ Configuration
- **LoRa Address**: Set Region.Community.Node
- **WiFi Settings**: Configure internet connection
- **Password Management**: Change AP password securely

## 🔌 API Reference

### Base URL
```
http://192.168.4.1/api/
```

### Authentication
All API endpoints require connection to the RadioDoge WiFi network.

### Endpoints

#### Device Status
```http
GET /api/status
```
Returns device status and configuration.

#### LoRa Address Management
```http
GET /api/address
POST /api/address
```
Get or set LoRa network address.

**POST Parameters:**
- `region`: Region number (0-255)
- `community`: Community number (0-255)  
- `node`: Node number (0-255)

#### Message Operations
```http
POST /api/message
```
Send text message via LoRa.

**Parameters:**
- `address`: Target address (e.g., "10.1.2")
- `type`: Message type ("text", "data", "command")
- `text`: Message content

#### Transaction Operations
```http
POST /api/transaction
```
Send Dogecoin transaction via LoRa.

**Parameters:**
- `address`: Target address (e.g., "10.1.2")
- `type`: Transaction type ("signed", "raw", "utxo")
- `data`: Transaction data (hex string)

#### Broadcast Operations
```http
POST /api/broadcast
```
Broadcast message to all devices.

**Parameters:**
- `type`: Broadcast type ("announcement", "emergency", "network", "transaction")
- `priority`: Priority level ("normal", "high", "urgent")
- `message`: Broadcast content

#### Ping Operations
```http
GET /api/ping
POST /api/ping
```
Test connectivity between devices.

**Parameters:**
- `region`: Target region (0-255)
- `community`: Target community (0-255)
- `node`: Target node (0-255)
- `broadcast`: Set to "1" for broadcast ping

#### WiFi Management
```http
GET /api/wifi
POST /api/wifi/connect
POST /api/wifi/disconnect
POST /api/wifi/clear
```
Manage internet WiFi connection.

**Connect Parameters:**
- `ssid`: WiFi network name
- `password`: WiFi password

#### Password Management
```http
POST /api/password/change
POST /api/password/reset
GET /api/password/status
```
Manage access point password.

**Change Parameters:**
- `password`: New password (8-32 chars, letters + numbers)

#### Gateway Operations
```http
POST /api/gateway
```
Send transaction to internet gateway.

**Parameters:**
- `url`: Gateway URL (optional, defaults to BlockCypher)
- `transaction`: Transaction data (hex string)

## 💻 Code Examples

### JavaScript Examples

#### Send a Message
```javascript
async function sendMessage() {
  const response = await fetch('http://192.168.4.1/api/message', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
    },
    body: 'address=10.1.2&type=text&text=Hello from JavaScript!'
  });
  
  const result = await response.json();
  console.log(result);
}
```

#### Send Dogecoin Transaction
```javascript
async function sendTransaction() {
  const transactionData = "0100000001..."; // Your signed transaction hex
  
  const response = await fetch('http://192.168.4.1/api/transaction', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
    },
    body: `address=10.1.2&type=signed&data=${transactionData}`
  });
  
  const result = await response.json();
  console.log(result);
}
```

#### Broadcast Message
```javascript
async function broadcastMessage() {
  const response = await fetch('http://192.168.4.1/api/broadcast', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
    },
    body: 'type=announcement&priority=normal&message=Network update!'
  });
  
  const result = await response.json();
  console.log(result);
}
```

#### Change WiFi Password
```javascript
async function changePassword() {
  const response = await fetch('http://192.168.4.1/api/password/change', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
    },
    body: 'password=MySecurePassword123'
  });
  
  const result = await response.json();
  console.log(result);
}
```

#### Get Device Status
```javascript
async function getStatus() {
  const response = await fetch('http://192.168.4.1/api/status');
  const status = await response.json();
  console.log('Device Status:', status);
}
```

### cURL Examples

#### Send a Message
```bash
curl -X POST "http://192.168.4.1/api/message" \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "address=10.1.2&type=text&text=Hello from cURL!"
```

#### Send Dogecoin Transaction
```bash
curl -X POST "http://192.168.4.1/api/transaction" \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "address=10.1.2&type=signed&data=0100000001..."
```

#### Broadcast Message
```bash
curl -X POST "http://192.168.4.1/api/broadcast" \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "type=emergency&priority=high&message=Emergency broadcast!"
```

#### Ping Device
```bash
curl -X GET "http://192.168.4.1/api/ping?region=10&community=1&node=2"
```

#### Connect to Internet WiFi
```bash
curl -X POST "http://192.168.4.1/api/wifi/connect" \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "ssid=MyWiFi&password=MyPassword"
```

#### Change AP Password
```bash
curl -X POST "http://192.168.4.1/api/password/change" \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "password=NewSecurePassword123"
```

#### Get Device Status
```bash
curl -X GET "http://192.168.4.1/api/status"
```

## 🔧 Configuration

### LoRa Network Address
- **Format**: Region.Community.Node (e.g., 10.1.3)
- **Range**: Each component 0-255
- **Default**: 10.1.1
- **Persistence**: Stored in NVS, survives reboots
- **Auto-Restore**: Last configuration automatically restored on boot
- **Management**: Set via web interface or API

### WiFi Configuration
- **AP Name**: RadioDoge (fixed)
- **AP Password**: Configurable (default: radiodoge)
- **Internet WiFi**: Configurable via web interface
- **Persistence**: Both stored in NVS
- **Auto-Connect**: Automatically connects to stored internet WiFi on boot
- **Dual Mode**: Simultaneous AP and internet connectivity
- **Status Display**: Real-time connection status (Online/Offline)

### Password Management
- **AP Password**: Configurable via web interface
- **Requirements**: 8-32 characters, must contain letters and numbers
- **Special Characters**: Allowed but not required
- **Case Sensitive**: Yes
- **Storage**: Password stored securely in NVS
- **Reset Option**: Can reset to default password
- **Validation**: Real-time password strength validation

### Persistent Storage Features
- **WiFi Credentials**: Internet WiFi SSID and password stored securely
- **LoRa Address**: Region, community, and node settings preserved
- **AP Password**: Custom access point password maintained
- **Auto-Restore**: All settings automatically restored on device restart
- **Clear Options**: Individual or complete configuration reset available

## 🆕 New Functionalities & Usage

### WiFi Internet Connectivity
The device can now connect to your local WiFi internet while maintaining its access point functionality:

**Purpose**: Enable internet connectivity for forwarding Dogecoin transactions to the blockchain network.

**How to Use**:
1. Connect to the RadioDoge access point
2. Open web interface at `http://192.168.4.1`
3. Go to "WiFi Configuration" section
4. Enter your internet WiFi SSID and password
5. Click "CONNECT TO INTERNET"
6. Device will automatically connect on future boots

**Benefits**:
- Forward transactions to Dogecoin network
- Access to external APIs and services
- Maintains local access point for device management
- Automatic reconnection on boot

### Persistent Configuration Storage
All device settings are now stored in non-volatile memory and automatically restored:

**What's Stored**:
- Internet WiFi credentials (SSID + password)
- LoRa network address (Region.Community.Node)
- Custom access point password

**How It Works**:
- Settings are saved automatically when changed
- Restored automatically on device boot
- No need to reconfigure after power cycles
- Individual settings can be cleared or reset

### Secure Password Management
The access point password can now be customized for enhanced security:

**Features**:
- Change password via web interface
- Strong password validation (8-32 chars, letters + numbers)
- Secure storage in NVS memory
- Reset to default option
- Real-time validation feedback

**Security Benefits**:
- Custom password for device access
- Same password for web interface and API
- Prevents unauthorized access
- Easy password management

### Enhanced Web Interface
The web interface now includes new sections and improved styling:

**New Sections**:
- **WiFi Configuration**: Manage internet connectivity
- **Access Point Password**: Change device password
- **Internet Gateway**: Configure transaction forwarding
- **Device Configuration**: Manage LoRa settings

**UI Improvements**:
- Cleaner status display (Online/Offline for internet)
- Improved password input styling
- SVG icons for better compatibility
- Grey buttons instead of red for better aesthetics
- White text on green buttons for better visibility

### Internet Gateway Integration
Transactions can now be automatically forwarded to the internet:

**Default Gateway**: BlockCypher API for Dogecoin transactions
**Custom Gateway**: Support for custom transaction endpoints
**Automatic Forwarding**: Transactions sent via LoRa are automatically forwarded to internet if connected
**Status Tracking**: Real-time feedback on internet forwarding success

## 🚨 Troubleshooting

### Common Issues

#### Device Won't Connect
- Check USB cable and drivers
- Try different USB port
- Hold BOOT button during upload

#### WiFi Connection Issues
- Verify password is correct
- Check if device is in range
- Try resetting to default password

#### Compilation Errors
- Ensure all libraries are installed
- Check Arduino IDE version compatibility
- Verify board selection

#### API Errors
- Ensure connected to RadioDoge WiFi
- Check API endpoint URLs
- Verify parameter format

### Reset to Defaults
1. **LoRa Config**: Use "Clear Stored Config" button
2. **WiFi Config**: Use "Clear Stored Credentials" button  
3. **Password**: Use "Reset to Default" button
4. **Full Reset**: Power cycle device

## 📊 Technical Specifications

### Hardware
- **MCU**: ESP32-S3
- **LoRa**: SX1262
- **Display**: 128x64 OLED
- **WiFi**: 802.11 b/g/n
- **Power**: USB or Battery

### LoRa Settings
- **Frequency**: 915 MHz (configurable)
- **Power**: 5 dBm
- **Bandwidth**: 125 kHz
- **Spreading Factor**: 7
- **Coding Rate**: 4/5

### Network
- **AP IP**: 192.168.4.1
- **Port**: 80 (HTTP)
- **Protocol**: HTTP/1.1
- **Format**: JSON responses

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🔗 Links

- **Heltec V3**: https://heltec.org/project/wifi-lora-32-v3/
- **Arduino IDE**: https://www.arduino.cc/en/software
- **ESP32 Documentation**: https://docs.espressif.com/projects/esp-idf/en/latest/

## 📞 Support

For issues and questions:
1. Check this README
2. Review troubleshooting section
3. Check GitHub issues
4. Create new issue with details

---

**RadioDoge V3 Firmware** - Secure Dogecoin transactions via LoRa mesh network 🌐🐕
