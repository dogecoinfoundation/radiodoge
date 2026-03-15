# 🐕 RadioDoge — Wireless P2P Dogecoin Over LoRa

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Rust](https://img.shields.io/badge/Built%20with-Rust%20🦀-orange)](https://rustlang.org)
[![Tauri 2](https://img.shields.io/badge/Tauri-v2-blue)](https://tauri.app)
[![Dogecoin](https://img.shields.io/badge/Powered%20by-Dogecoin-f7d02c?logo=dogecoin)](https://dogecoin.com)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Android-informational)]()
[![Windows CI](https://github.com/jnowat/RadioDoge/actions/workflows/build-windows.yml/badge.svg)](https://github.com/jnowat/RadioDoge/actions/workflows/build-windows.yml)
[![Android CI](https://github.com/jnowat/RadioDoge/actions/workflows/build-android.yml/badge.svg)](https://github.com/jnowat/RadioDoge/actions/workflows/build-android.yml)

> **Much wireless. Very transaction. Wow.** 🌙

Send and receive Dogecoin over **LoRa radio waves** — completely offline, no internet required. RadioDoge uses Heltec ESP32 boards with built-in SX1262 LoRa transceivers to create a wireless mesh network for Dogecoin transactions.

> **v0.3.9** ✨: Android is here! Install the debug APK on any Android 7.0+ device — Pixel, Samsung, whatever you've got. Mobile-responsive UI with icon-only NavBar on phones. Persistent wallet, battery display, light/dark theme, address book, gateway mode, WiFi toggle, mesh neighbor map — all shipped in v0.3.6–v0.3.9. Much mobile. Very LoRa. Wow!

---

## 🚀 Get the App — Windows MSI or Android APK

> **Builds trigger on every push — grab the MSI or APK from Actions artifacts instantly!**

### 🖥️ Windows MSI

#### ⚡ Option A: Instant Download (Every Push → Actions Artifacts)

1. Go to the [**Windows CI**](https://github.com/jnowat/RadioDoge/actions/workflows/build-windows.yml)
2. Click the latest **"🐕 Build Windows MSI"** run
3. Scroll to **Artifacts** → click **`RadioDoge-x.x.x-windows-x64-msi`** → download the `.zip`
4. Unzip → double-click the `.msi` → done! 🐕

#### 🏷️ Option B: Tagged Release (Permanent)

1. Go to [Releases](https://github.com/jnowat/RadioDoge/releases)
2. Download `RadioDoge_x.x.x_x64_en-US.msi` → install and launch

Artifacts kept for **30 days**. MSI auto-attaches to any GitHub Release you create.

---

### 📱 Android APK (Debug — Sideload)

Every push also triggers an Android build. The APK is signed with the Gradle debug key so it installs directly on any Android 7.0+ device.

1. Go to [**Android CI**](https://github.com/jnowat/RadioDoge/actions/workflows/build-android.yml)
2. Click the latest **"Android (APK)"** run
3. Scroll to **Artifacts** → click **`radiodoge-android-debug-apk-vx.x.x`** → download the `.zip`
4. Unzip → copy the `.apk` to your phone
5. Enable **Settings → Install unknown apps** for your file manager
6. Tap the `.apk` → Install → done! 🐕

> **Note**: This is a debug build for testing. USB serial to the Heltec via OTG is coming in a future release — the Android app currently covers wallet, history, and UI features.

---

## ✨ What Makes RadioDoge Special?

- **No internet required** — transactions travel over LoRa radio (up to 15 km range!)
- **Mesh networking** — packets hop between nodes to reach the nearest gateway
- **Beautiful GUI** — fun Dogecoin-themed app with confetti, TX/RX live log, and Easter egg 🎉
- **Pure Rust crypto** — no browser, no cloud, generate real Dogecoin keys locally
- **Open hardware** — works with standard Heltec ESP32 LoRa V3 boards (~$20)
- **Android app** — installable debug APK built by CI on every push, runs on Android 7.0+
- **Instant builds** — every push to master compiles a fresh Windows MSI and Android APK

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
- Windows 10/11 (x64) **or** Android 7.0+ device
- [Heltec ESP32 LoRa V3](https://heltec.org/project/wifi-lora-32-v3/) board
- USB cable (USB-C)
- [Node.js 20+](https://nodejs.org) and [Rust](https://rustup.rs) (for development only)

### Option A: Download the MSI (Windows) or APK (Android)

- **Windows**: Go to [Windows CI](https://github.com/jnowat/RadioDoge/actions/workflows/build-windows.yml) → latest run → Artifacts → download MSI zip → install
- **Android**: Go to [Android CI](https://github.com/jnowat/RadioDoge/actions/workflows/build-android.yml) → latest run → Artifacts → download APK zip → sideload

### Option B: Build from Source

```bash
# Clone (with submodules for libdogecoin)
git clone --recurse-submodules https://github.com/jnowat/RadioDoge.git
cd RadioDoge/radiodoge-gui

# Install frontend dependencies
npm ci

# Run in development mode (hot-reloading!)
cargo tauri dev
```

### Option C: Use the Headless CLI (no GUI needed)

The new `radiodoge-cli` replaces RadioDogeSharp (C#) and serdog (C) with a pure Rust binary:

```bash
# Build the CLI
cargo build -p radiodoge-cli --release

# List available serial ports
./target/release/radiodoge-cli ports

# Generate a Dogecoin wallet
./target/release/radiodoge-cli wallet generate

# Send DOGE over LoRa (no GUI required!)
./target/release/radiodoge-cli send -p COM3 -t DH5yaieq... -a 4.20 -m "much transfer"

# Listen for incoming LoRa packets
./target/release/radiodoge-cli receive -p COM3 --timeout 60

# Run as a headless gateway daemon (replaces serdog)
./target/release/radiodoge-cli daemon -p /dev/ttyUSB0
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

## 🛠️ How to Test with Real Hardware

> **Everything below applies once you have a flashed Heltec ESP32 LoRa V3 in hand.**
> The app is fully functional — zero placeholders.

### Step 1 — Flash the Heltec Firmware

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software) and open it
2. Go to **File → Preferences** and paste into "Additional boards manager URLs":
   ```
   https://resource.heltec.cn/download/package_heltec_esp32_index.json
   ```
3. Go to **Tools → Board → Board Manager**, search **Heltec ESP32**, install
4. Install libraries via **Sketch → Include Library → Manage Libraries**:
   - `LoRaWan_APP` (Heltec)
   - `Adafruit GFX Library`
   - `Adafruit SSD1306`
5. Open `heltec-firmware-v3/heltec-firmware.ino`
6. Select board: **Heltec WiFi LoRa 32(V3)**, choose the correct COM port
7. Click **Upload** (upload speed 921600 works reliably)
8. On success, the OLED shows the node address (e.g., `10.0.1`) — the radio is live!

### Step 2 — Connect in the GUI

1. Plug the flashed Heltec into your PC via USB-C
2. Launch RadioDoge (MSI installer or `cargo tauri dev` in `radiodoge-gui/`)
3. On the **Connect** tab, click **⟳ Refresh** — the Heltec COM port appears (usually `COM3`–`COM8` on Windows)
4. Select the port, click **🔌 Connect to Heltec**
5. The app pings the device: if it responds within 500 ms you'll see **✅ Connected!** with RSSI and the node address
6. The NavBar turns green and the Dashboard tab becomes active automatically

### Step 3 — Generate a Dogecoin Wallet

1. Click the **Wallet** tab
2. Click **Generate New Wallet** — pure Rust crypto creates a real mainnet keypair in < 1 ms
3. Your address starts with `D` (e.g., `DH5yaieqoZN36fDVciNyRueRGvGLR3mr7L`)
4. **Save your private key (WIF) now** — it is never stored to disk! Copy it somewhere safe

### Step 4 — Send a Dogecoin Transaction Over LoRa

1. Click the **Send** tab
2. Paste the recipient's Dogecoin address (must start with `D`)
3. Enter the amount (e.g., `1.00`) and an optional memo (e.g., `such payment, wow`)
4. Click **🚀 Sign & Broadcast via Radio**
5. The app encodes the payload, splits it into LoRa packets if > 192 bytes, and sends each over the serial port
6. The Heltec device broadcasts the packet over the LoRa mesh
7. 🎉 Confetti explodes and a success toast appears on screen!

### Step 5 — Monitor Incoming Packets

- **Dashboard tab**: live RSSI/SNR meters + packet log update every 2 s
- **Receive tab**: decoded incoming LoRa packets with source address, command, and RSSI
- **Signal Bars** in the NavBar and Dashboard reflect real RF conditions

### Troubleshooting

| Symptom | Fix |
|---|---|
| "No ports found" | Check USB cable, install CP210x or CH340 drivers, click ⟳ Refresh |
| "Not connected" after clicking Connect | Wrong port selected, or firmware not flashed — check OLED on device |
| RSSI very low (< −100 dBm) | Move nodes closer, or adjust spreading factor in Settings tab |
| Transaction not received by gateway | No gateway node in range — add a second Heltec as a relay/gateway |
| SmartScreen warning on MSI | Click "More info → Run anyway" — unsigned build is normal for dev |

---

## 🗂️ Project Structure

```
radiodoge/
├── Cargo.toml               🆕 Cargo workspace root (all Rust crates)
├── .github/workflows/
│   ├── build-windows.yml    Push-triggered MSI builds (every push to master!)
│   └── build-android.yml    Push-triggered Android APK builds (debug, sideloadable)
├── crates/
│   ├── radiodoge-core/      🆕 Pure Rust shared library (no Tauri, no GUI)
│   │   └── src/             types, wallet, radio protocol, serial port manager
│   └── radiodoge-cli/       🆕 Headless CLI (Rust port of RadioDogeSharp + serdog)
│       └── src/main.rs      ports, wallet, send, receive, ping, connect, daemon
├── radiodoge-gui/           Rust + Tauri 2 desktop GUI (PRIMARY)
│   ├── src/                 Svelte 5 + TypeScript frontend
│   └── src-tauri/           Rust Tauri backend (uses radiodoge-core)
├── heltec-firmware-v3/      Arduino firmware for Heltec ESP32 (primary)
├── heltec-firmware/         Arduino firmware v2 (prototype)
├── RadioDogeSharp/          C# .NET console app (legacy — replaced by radiodoge-cli)
├── serdog/                  C serial daemon (legacy — replaced by radiodoge-cli daemon)
├── libdogecoin/             Dogecoin cryptography library (legacy — replaced by pure Rust wallet)
└── docs/                    Documentation
```

### The GUI (`radiodoge-gui/`)

The Rust + Tauri 2 GUI is the **primary official client**. It features:

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
| Mobile | Tauri Mobile (Android APK via `tauri android build`) |
| CI/CD | GitHub Actions → Windows MSI + Android APK on every push |

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

## 🏗️ CI/CD — MSI + APK Auto-Builds on Every Push

Two GitHub Actions workflows trigger automatically on every push:

### Windows MSI (`.github/workflows/build-windows.yml`)

| Event | What Happens |
|---|---|
| **Push to any branch** | Builds MSI → uploads as Actions artifact (download immediately!) |
| **GitHub Release created** | Builds MSI → attaches to the release permanently |
| **Manual dispatch** | Builds MSI → uploads as artifact |

### Android APK (`.github/workflows/build-android.yml`)

| Event | What Happens |
|---|---|
| **Push to any branch** | Builds debug APK → uploads as Actions artifact |
| **GitHub Release created** | Builds debug APK → uploads as artifact |
| **Manual dispatch** | Builds debug APK → uploads as artifact |

### Optional: Code Signing (for production)

```bash
# Generate a signing keypair
cargo tauri signer generate -w ~/.tauri/radiodoge.key

# Add these as GitHub repository secrets (Settings → Secrets → Actions):
#   TAURI_SIGNING_PRIVATE_KEY          (base64 contents of .key file)
#   TAURI_SIGNING_PRIVATE_KEY_PASSWORD (your passphrase)
```

Unsigned builds work fine for development and testing — Windows may show a SmartScreen warning on first run, which you can bypass with "More info → Run anyway".

---

## 🗺️ Development Roadmap

> **Much ambition. Very phases. Such plan. Wow. 🐕**

---

### ✅ v0.2.x — Windows Foundation (Complete)

The bedrock. Everything that makes a production-ready installable app:

- ✅ Tauri 2 + Svelte 5 GUI with full Dogecoin yellow/orange dark theme
- ✅ Pure Rust Dogecoin wallet generation (`secp256k1` + `sha2` + `ripemd` + `bs58`) — real mainnet `D...` addresses, no FFI
- ✅ Real serial communication with Heltec ESP32 at 115,200 baud (`serialport` crate)
- ✅ RadioDoge binary packet protocol: `PING`, `GET_ADDR`, `SET_ADDR`, `DOGE_TX`, `MULTIPART`
- ✅ Async Tokio I/O with `spawn_blocking` (blocking reads never stall the Tokio thread pool)
- ✅ Multipart packets for payloads > 192 bytes (was silently truncated — fixed in v0.2.4)
- ✅ Live RSSI/SNR stats polling + 5-bar signal widget updated every 2 s
- ✅ Live packet monitor (Receive tab) with decoded payload display
- ✅ Windows x64 MSI produced by GitHub Actions on every push to `master`
- ✅ Cargo workspace: three crates (`radiodoge-core`, `radiodoge-cli`, GUI backend)
- ✅ `radiodoge-cli` — headless Rust CLI (ports, wallet, send, receive, ping, daemon)

---

### ✅ v0.3.x — Heltec Polish + Real-World UX + Android (Complete)

Rock-solid when you plug in a real Heltec board, now with Android support:

- ✅ Epic Doge-boombox hero image throughout (NavBar, Connect, Dashboard, MSI icon)
- ✅ Smart USB port detection — CP210x, CH340, CH9102, FTDI, Espressif native flagged `🟢 Heltec`
- ✅ Ports sorted: likely-Heltec first → other USB → native COM; auto-selected on refresh
- ✅ **Ping Device** button with millisecond round-trip readout
- ✅ Actionable error messages: access-denied, driver-missing, cable-failure with driver URLs
- ✅ Non-USB port selection warning
- ✅ **Firmware version badge** — queries `CMD_GET_FIRMWARE_VERSION` on connect, shows `FW v1.x.x`
- ✅ **Auto-reconnect** — detects USB re-plug → retries with 5s→10s→20s→60s exponential backoff
- ✅ **Save to Device round-trip verification** — sends settings → queries device → shows `Verified ✓`
- ✅ **Desktop OS toast** for incoming DOGE TX packets (tauri-plugin-notification)
- ✅ **Copy-to-clipboard** on every address, key, packet field, and node address
- ✅ **Show Address QR button** — dedicated toggle separate from private key reveal
- ✅ **TX/RX packet labels** — green ↑ TX / blue ↓ RX direction icons with filter toggles
- ✅ **Dynamic signal descriptions** — "Much Strong Radio!", "Very Weak — Such Distance"
- ✅ **Tooltips everywhere** — all cards, sliders, buttons, and fields have helpful + funny titles
- ✅ **Branding fix** — NavBar now consistently shows "RadioDoge" (was "DOGE RADIO")
- ✅ **Accessibility** — ARIA labels, focus rings, role attributes, live regions
- ✅ **Easter egg** 🎮 — "such wow" toast (use the ultimate [read "uber-nostalgic"] cheat code!)
- ✅ **Board sync** — app queries `CMD_GET_SETTINGS` (0x22) on connect; board is source of truth for node address and gateway mode
- ✅ **Gateway mode** — enable/disable gateway from the app; gateway daemon status badge in NavBar
- ✅ **WiFi toggle** — enable/disable the board's WiFi radio from Settings
- ✅ **Mesh neighbor map** — tracks recently heard nodes; address conflict detection (CMD 0x25)
- ✅ **Address book** — save and label Dogecoin addresses, persisted to `address_book.json`
- ✅ **Persistent wallet** — save encrypted wallet to disk; reload on next launch
- ✅ **Battery voltage display** — polls board every 15 s when connected
- ✅ **Board MAC address** — displayed in Settings tab
- ✅ **Light/dark theme toggle** — full theme switcher in Settings
- ✅ **Android app** — Tauri Mobile port; debug APK built by CI on every push, installable on Android 7.0+ (API 24); mobile-responsive UI with icon-only NavBar on phones

---

### 🚀 v0.4.x — Full Dogecoin Transactions Over LoRa

End-to-end on-chain transactions — no internet required on your device:

- 🔜 **Dogecoin Core RPC bridge (optional)** — when Core is running on the same machine, RadioDoge can discover existing wallets/addresses/balances via localhost RPC, use them for signing, then fall back to pure offline LoRa mode (read-only RPC by default for maximum safety)
- 🔜 **UTXO fetching via gateway** — broadcast `REQUEST_BALANCE`; gateway queries Dogecoin RPC and relays UTXO set back over LoRa mesh
- 🔜 **BIP32/BIP44 HD wallet** — derive multiple addresses from a single mnemonic seed (m/44'/3'/0'/0/n)
- 🔜 **Full raw transaction signing** — construct + sign a valid Dogecoin transaction in pure Rust; no internet touched
- 🔜 **Transaction confirmation feedback** — gateway ACKs broadcast and reports `txid` back over LoRa
- 🔜 **SPV verification** — lightweight header chain validation; app can verify inclusion without a full node
- 🔜 **Multi-hop relay status** — show hop count and intermediate node addresses in the packet log
- 🔜 **Address book** — save labelled Dogecoin addresses in encrypted local storage
- 🔜 **QR code scanning** — camera/image input for recipient field (no hand-typing long addresses)
- 🔜 **Fee estimation** — gateway reports current mempool fee rate; app sets appropriate sat/byte fee

---

### 📡 v0.5.x — Meshtastic Mesh Interoperability

Bridge RadioDoge with the existing Meshtastic community:

- 🔜 **Meshtastic packet encoding** — wrap RadioDoge payloads in Meshtastic protobuf so standard Meshtastic nodes can relay them
- 🔜 **Detect Meshtastic nodes** — scan for nodes on the same LoRa channel; display in a network map view
- 🔜 **Bridge mode** — `radiodoge-cli daemon` bridges between RadioDoge and Meshtastic protocols transparently
- 🔜 **Multi-channel scanning** — hop between frequencies/SFs to reach nodes on different Meshtastic presets
- 🔜 **GPS location embedding** — include sender coordinates in packets for disaster-response positioning
- 🔜 **Store-and-forward** — nodes cache undelivered packets and retry when destination node is seen

---

### 📱 Future Horizons

- ✅ **Android app via Tauri Mobile** — debug APK ships on every CI push; installable on Android 7.0+ (USB-OTG serial connection to Heltec coming next)
- 🔜 **Android USB-OTG serial** — connect to Heltec directly from your phone via USB cable
- 🔜 **iOS** — Bluetooth LE to Heltec via BLE-serial bridge firmware
- 🔜 **Linux AppImage + macOS .dmg** in CI — Tauri supports these targets already
- 🔜 **WebAssembly packet inspector** — browser tool to decode RadioDoge packets from hex
- 🔜 **Gateway dashboard** — monitor multiple gateway nodes, relay stats, mesh health at a glance
- 🔜 **Lightning over LoRa** — forward BOLT-11 invoices + HTLC state through the mesh for instant DOGE

---

## 🏛️ Legacy: RadioDogeSharp

The `RadioDogeSharp/` directory contains the original Windows C# .NET 6.0 console application. It remains as a **reference implementation** showing the serial protocol and SPV wallet logic in C#. New development happens in `radiodoge-gui/`.

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

*Much open source. Very community. Such LoRa. Wow. 🐕🌙*
