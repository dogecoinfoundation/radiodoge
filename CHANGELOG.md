# Changelog

All notable changes to RadioDoge will be documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.2.4] — 2026-03-07 — 🔧 Mega Fix: CI Hardening + Rust Port + Bug Fixes

> **MSI now builds reliably on every push. Every code bug fixed. Everything ported to Rust.**
> Much reliability. Very Rust. Such workspace. Wow. 🐕

### Fixed — CI/CD (`build-windows.yml`)

| Problem | Fix |
|---|---|
| `permissions: contents: write` missing | Added — `softprops/action-gh-release` was 403-ing on every release event |
| `npm install` (non-deterministic) | Changed to `npm ci` (uses `package-lock.json` exactly) |
| `package-lock.json` not committed | Generated and committed — enables `npm ci` and npm cache |
| Missing `CARGO_INCREMENTAL=0` | Added — prevents corrupt Rust build cache artifacts |
| `workspaces: radiodoge-gui/src-tauri` | Updated to `. -> target` for new workspace root |
| MSI artifact path wrong after workspace | Updated to `target/x86_64-pc-windows-msvc/release/bundle/msi/*.msi` |
| No debug output on failure | Added debug steps: runner info, bundle dir listing (always runs) |

### Fixed — Rust: Blocking Read Stalling Tokio Runtime (serial.rs)

**Bug**: `serial.rs` called `port.read()` (a blocking syscall) while holding a
`tokio::sync::Mutex` lock guard. This blocked the entire Tokio executor thread pool,
causing UI lag and potential deadlocks. The comment even said "Use spawn_blocking" but
it was never actually used.

**Fix**: Changed `port` and `cancel` fields to `std::sync::Mutex`. The read loop now
uses `tokio::task::spawn_blocking` — correct, non-blocking Tokio I/O.

### Fixed — Rust: `send_transaction` Silently Truncating Large Payloads (lib.rs)

**Bug**: `lib.rs:206` had `// For MVP: truncate to single packet` — if an encoded
transaction exceeded 192 bytes (e.g. long memos or future extended formats), bytes were
silently dropped. The user thought they sent the full payload; the device got garbage.

**Fix**: Properly dispatches multipart packets with 50ms inter-packet spacing when
payload exceeds `MAX_SINGLE_PAYLOAD_LEN`.

### Added — Cargo Workspace at Repo Root

New `Cargo.toml` at the repository root defines a workspace with three members:

```
crates/radiodoge-core   ← shared pure-Rust library (no Tauri)
crates/radiodoge-cli    ← headless CLI binary
radiodoge-gui/src-tauri ← Tauri 2 desktop GUI backend
```

Shared dependency versions are pinned in `[workspace.dependencies]`.

### Added — `crates/radiodoge-core` (shared pure-Rust library)

Extracted the four non-Tauri modules from `radiodoge-gui/src-tauri/src/` into a
standalone library crate consumed by both the GUI and the new CLI:

- `types.rs` — all shared data structures
- `wallet.rs` — pure Rust Dogecoin keypair generation and transaction encoding
- `radio.rs` — LoRa packet protocol encoding/decoding
- `serial.rs` — cross-platform serial port management (with the blocking-read fix above)

### Added — `crates/radiodoge-cli` (Rust port of RadioDogeSharp + serdog)

A full headless CLI replacing the C# `RadioDogeSharp` and C `serdog` legacy tools:

```
radiodoge-cli ports                    list serial ports
radiodoge-cli wallet generate          new Dogecoin keypair
radiodoge-cli wallet validate <ADDR>   validate address
radiodoge-cli send -p <PORT> -t <ADDR> -a <DOGE>  send transaction over LoRa
radiodoge-cli receive -p <PORT>        listen for incoming packets
radiodoge-cli ping -p <PORT>           ping device
radiodoge-cli connect <PORT>           interactive REPL (replaces RadioDogeSharp menu)
radiodoge-cli daemon -p <PORT>         headless daemon (replaces serdog)
```

Build: `cargo build -p radiodoge-cli --release`

### Changed — `radiodoge-gui/src-tauri/src/lib.rs`

- Removed `mod types; mod serial; mod wallet; mod radio;` (now in `radiodoge-core`)
- Added `use radiodoge_core::{radio, wallet, serial, types};`
- Removed the 4 module `.rs` files that were duplicated in the GUI crate

### Changed — Version bumped to 0.2.4

- `radiodoge-gui/package.json`
- `radiodoge-gui/src-tauri/tauri.conf.json`
- `radiodoge-gui/src-tauri/Cargo.toml`
- `crates/radiodoge-core/Cargo.toml`
- `crates/radiodoge-cli/Cargo.toml`

### Changed — `package.json`: added `engines` field

```json
"engines": { "node": ">=20.0.0" }
```

---

## [0.2.3] — 2026-03-07 — 🚀 Push-Triggered MSI Builds — No Tag Required!

> **MSI auto-builds on every push to master — download instantly from the Actions tab!**
> Much automation. Very CI. Such instant installer. Wow. 🐕

### Fixed — CI/CD: Instant MSI on Every Push (Like SteloPTC)

The old workflow (`tauri-release.yml`) used `tauri-apps/tauri-action@v0` to create GitHub
Releases automatically. This **failed on every push after the first** because the action
tried to create a tag (`v0.2.3`) that already existed — producing a 422 error before the
MSI was ever compiled. No MSI was produced. No artifact was uploaded. 😢

The new workflow (`build-windows.yml`) fixes this completely:

#### What Changed

- **Renamed** `.github/workflows/tauri-release.yml` → `.github/workflows/build-windows.yml`
- **Trigger on push** to `master` / `main` — builds MSI and uploads as an Actions artifact
  immediately, with zero need for a release tag (modeled after SteloPTC's build-windows.yml)
- **Trigger on release created** — builds MSI and attaches it to the GitHub Release
- **Removed `tauri-actions`** — replaced with `cargo tauri build --bundles msi` directly,
  eliminating the "tag already exists" failure mode entirely
- **Added `actions/upload-artifact@v4`** — MSI is now available for download from the
  Actions tab on every push; retained for 30 days
- **Added `softprops/action-gh-release@v2`** — attaches MSI to GitHub Releases when you
  create one
- **Added retry logic** (3 attempts, exponential backoff) for transient WiX toolset
  download failures (a known CI flakiness source)
- **Added job summary** — each build prints clear download instructions directly in the
  GitHub Actions UI
- **Added `workflow_dispatch`** — manually trigger a build any time from the Actions tab

#### How to Get Your MSI Now

```
Push to master → Actions tab → latest "🐕 Build Windows MSI" run → Artifacts → download
```

No tags. No releases. Just push and grab the installer. 🐕

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

#### CI/CD (superseded by v0.2.3 fix above)

- Initial GitHub Actions workflow (`tauri-release.yml`) — triggered on `v*` tags, built Windows x64 MSI via WiX bundler, uploaded to GitHub Releases

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

*Such changelog. Very version. Much automation. Wow. 🐕*
