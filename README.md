# 🐕 RadioDoge — Wireless P2P Dogecoin Over LoRa

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Rust](https://img.shields.io/badge/Built%20with-Rust%20🦀-orange)](https://rustlang.org)
[![Tauri 2](https://img.shields.io/badge/Tauri-v2-blue)](https://tauri.app)
[![Dogecoin](https://img.shields.io/badge/Powered%20by-Dogecoin-f7d02c?logo=dogecoin)](https://dogecoin.com)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20Android%20(soon)-informational)]()

> **Much wireless. Very transaction. Wow.** 🌙

Send and receive Dogecoin over **LoRa radio waves** — completely offline, no internet required. RadioDoge uses Heltec ESP32 boards with built-in SX1262 LoRa transceivers to create a wireless mesh network for Dogecoin transactions.

---

## 🚀 One-Click Install (Windows)

Download the latest `.msi` installer from the [**Releases page**](https://github.com/jnowat/RadioDoge/releases) and double-click to install.

No dependencies, no command line, just Dogecoin over the airwaves. 🐕📡

---

## ✨ What Makes RadioDoge Special?

- **No internet required** — transactions travel over LoRa radio (up to 15km range!)
- **Mesh networking** — packets hop between nodes to reach the nearest gateway
- **Beautiful GUI** — fun Dogecoin-themed desktop app with confetti on every transaction 🎉
- **Pure Rust crypto** — no browser, no cloud, generate real Dogecoin keys locally
- **Open hardware** — works with standard Heltec ESP32 LoRa V3 boards (~$20)

---

## 🔧 How RadioDoge Works

```
Your PC / Phone
     │ USB Serial
     ▼
[Heltec ESP32 V3]  ← LoRa SX1262 Radio
     │
     │  LoRa 915 MHz radio waves ~~~~~~~~~~~~~~~~~~~~
     │
     ▼
[Other Heltec Nodes]  →  [Gateway Node]
                                │
                                │ Internet
                                ▼
                        [Dogecoin Network]
```

1. Your desktop app (RadioDoge GUI) sends a signed transaction to the Heltec device via USB serial
2. The device broadcasts it over LoRa radio to nearby nodes
3. Nodes relay the packet through the mesh (up to 3 hops)
4. A gateway node (connected to internet) forwards it to the Dogecoin network
5. 🎉 Your transaction is confirmed on-chain!

---

## ⚡ Getting Started in 5 Minutes

### Prerequisites
- Windows 10/11 (x64) or Linux
- [Heltec ESP32 LoRa V3](https://heltec.org/project/wifi-lora-32-v3/) board
- USB cable (USB-C)
- [Node.js 20+](https://nodejs.org) and [Rust](https://rustup.rs) (for development)

### Option A: Download the MSI (Windows, easiest)

1. Go to [Releases](https://github.com/jnowat/RadioDoge/releases)
2. Download `RadioDoge_x.x.x_x64_en-US.msi`
3. Install and launch — done! 🐕

### Option B: Build from Source

```bash
# Clone (with submodules for libdogecoin)
git clone --recurse-submodules https://github.com/jnowat/RadioDoge.git
cd RadioDoge/radiodoge-gui

# Install frontend dependencies
npm install

# Run in development mode (hot-reloading!)
cargo tauri dev
```

### Flash the Heltec Firmware

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. Add the Heltec ESP32 board package (URL in Arduino IDE preferences):
   ```
   https://resource.heltec.cn/download/package_heltec_esp32_index.json
   ```
3. Install libraries: `Adafruit GFX`, `Adafruit SSD1306`, `LoRaWan_APP`
4. Open `heltec-firmware-v3/heltec-firmware.ino` in Arduino IDE
5. Select board: **Heltec WiFi LoRa 32(V3)**
6. Upload at 921600 baud
7. The OLED display will show the node address on boot

---

## 🗂️ Project Structure

```
radiodoge/
├── radiodoge-gui/           🆕 Rust + Tauri 2 desktop GUI (PRIMARY)
│   ├── src/                 Svelte 5 + TypeScript frontend
│   └── src-tauri/           Rust backend (serial, wallet, radio protocol)
├── heltec-firmware-v3/      Arduino firmware for Heltec ESP32 (primary)
├── heltec-firmware/         Arduino firmware v2 (prototype)
├── RadioDogeSharp/          C# .NET console app (legacy, Windows)
├── serdog/                  C serial daemon (Linux legacy)
├── libdogecoin/             Dogecoin cryptography library (git submodule)
└── docs/                    Documentation
```

### The New GUI (`radiodoge-gui/`)

The Rust + Tauri 2 GUI is the **primary official client** going forward. It features:

- **Svelte 5** frontend with reactive runes
- **TailwindCSS v4** with a custom Doge yellow/orange dark theme
- **Pure Rust Dogecoin wallet** — generate real `D...` addresses with `secp256k1`
- **Cross-platform serial** via the `serialport` crate (Windows, Linux, macOS)
- **Confetti explosion** on every successful transaction 🎉
- **System tray** icon with connection status
- **Animated signal bars**, live RSSI/SNR meters, real-time packet log

### Tech Stack

| Layer | Technology |
|-------|-----------|
| UI Framework | Svelte 5 + TypeScript |
| CSS | TailwindCSS v4 (CSS-first config) |
| Desktop Shell | Tauri 2 |
| Rust Runtime | Tokio (async) |
| Serial Comms | `serialport` crate |
| Dogecoin Crypto | `secp256k1` + `sha2` + `ripemd` + `bs58` |
| CI/CD | GitHub Actions → Windows MSI |

---

## 🔌 Serial Protocol

The GUI communicates with the Heltec device at **115200 baud** using RadioDoge's binary packet format:

```
Byte 0: Command (0x00=GET_ADDR, 0x02=PING, 0x03=MESSAGE, 0x10=DOGE_TX, ...)
Byte 1: Flags (0x00=single, 0x01=multipart)
Byte 2: Source region
Byte 3: Source community
Byte 4: Source node
Byte 5: Destination region
Byte 6: Destination community
Byte 7: Destination node
Byte 8+: Payload (up to 192 bytes)
```

Node addresses use the `Region.Community.Node` format (e.g., `10.0.2`).

---

## 🏗️ Building the MSI Installer

GitHub Actions automatically builds the MSI when you push a `v*` tag:

```bash
git tag v0.2.0
git push origin v0.2.0
```

To set up signing keys (recommended for production):

```bash
# Generate a signing keypair
cargo tauri signer generate -w ~/.tauri/radiodoge.key

# Add these as GitHub repository secrets:
#   TAURI_SIGNING_PRIVATE_KEY         (contents of the .key file, base64)
#   TAURI_SIGNING_PRIVATE_KEY_PASSWORD (your passphrase)
```

---

## 📱 Future: Android App

The Rust core in `radiodoge-gui/src-tauri/src/` is intentionally written with **no platform-specific code**. The serial communication will be adapted to use `serialport-android` for USB-OTG connectivity. Watch this space for a Tauri Mobile Android port! 🤖

---

## 🏛️ Legacy: RadioDogeSharp

The `RadioDogeSharp/` directory contains the original Windows C# .NET 6.0 console application. It remains in the repository as a **reference implementation** showing the serial protocol and SPV wallet logic in C#. New development happens in `radiodoge-gui/`.

---

## 🤝 Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/much-wow`)
3. Commit with clear messages (`git commit -m 'Add such feature'`)
4. Push and open a Pull Request

---

## 📄 License

MIT © RadioDoge Contributors

---

*Much open source. Very community. Wow. 🐕🌙*
