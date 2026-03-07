# Changelog

All notable changes to RadioDoge will be documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.2.2] — 2026-03-07 — 🐕 Rust GUI Launch – Much Wow, Very Installable!

### Added — Beautiful Windows + Android-Ready Desktop App

This release introduces a **complete rewrite of the Windows desktop client** as a
production-ready Rust + Tauri 2 application (`radiodoge-gui/`). The C# console
app (`RadioDogeSharp/`) remains in the repository as a legacy reference.

#### New: `radiodoge-gui/` (primary client)

- **Tauri 2 desktop application** targeting Windows x64 (Linux and macOS bonus targets)
- **Svelte 5 frontend** with TypeScript and reactive runes (`$state`, `$derived`, `$effect`)
- **TailwindCSS v4** (CSS-first configuration) with custom Doge yellow/orange dark theme
- **Fun Dogecoin UI** — bouncing Shiba Inu emoji, "much wow" copy, confetti explosion on successful transactions
- **System tray icon** with connection status and quick actions

#### Features

- **Connect tab** — auto-detect COM ports, animated connect button, live signal bars
- **Dashboard tab** — radio stats grid (frequency, power, SF, BW, packets sent/received), RSSI/SNR meter, live packet log
- **Wallet tab** — generate Dogecoin keypair (pure Rust, no FFI), QR code display, copy buttons, private key reveal toggle
- **Send tab** — recipient address, amount, memo field; "Sign & Broadcast via Radio" button; confetti 🎉 + success message on send
- **Receive tab** — live packet monitor with decoded transactions, source addresses, RSSI
- **Settings tab** — LoRa parameters (frequency, power, SF, BW, coding rate), node address config, save to device

#### Technical Highlights

- **Pure Rust Dogecoin wallet** using `secp256k1` crate — generates real mainnet `D...` addresses with correct version byte `0x1E`; WIF private keys with version byte `0x9E`
- **Cross-platform serial** via `serialport` crate — no platform-specific code; architecture ready for Android via `serialport-android`
- **Async Tokio runtime** with `tokio::task::spawn_blocking` for serial I/O
- **Tauri events** for real-time packet and stats updates (`radio-packet`, `radio-stats-update`, `connection-status`, `transaction-sent`)
- **RadioDoge packet protocol** fully implemented in Rust matching the existing C# and C implementations

#### CI/CD

- **GitHub Actions** workflow (`.github/workflows/tauri-release.yml`) — triggers on `v*` tags, builds Windows x64 MSI via WiX bundler, uploads to GitHub Releases
- Rust build cache (`swatinem/rust-cache`) for fast CI

### Unchanged (legacy preserved)

- `RadioDogeSharp/` — C# .NET 6.0 console application (reference implementation)
- `serdog/` — C serial daemon for Linux
- `heltec-firmware/` — Arduino v2 prototype firmware
- `heltec-firmware-v3/` — Arduino v3 production firmware with REST API
- `libdogecoin/` — Dogecoin C library git submodule

---

## [0.1.0] — 2023 — Initial Release

### Added

- `heltec-firmware/` — Initial Heltec ESP32 LoRa firmware prototype
- `RadioDogeSharp/` — Windows C# .NET 6.0 console client
  - Serial port communication at 115200 baud
  - RadioDoge mesh network addressing (Region.Community.Node)
  - SPV wallet mode with libdogecoin bindings
  - Multipart packet support for large payloads
  - Demo mode for testing without hardware
- `serdog/` — Linux C serial daemon with UTXO management
- `libdogecoin/` — Dogecoin cryptography library (git submodule)
- `docs/` — Design documentation

---

*Such changelog. Very version. Wow. 🐕*
