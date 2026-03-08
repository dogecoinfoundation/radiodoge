# Changelog

All notable changes to RadioDoge will be documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.3.1] — 2026-03-08 — ✨ Full Polish Release — Much Wow Delivered

> **The most delightful RadioDoge release yet. Every feature sharpened, every interaction refined.**
> Much polish. Very smooth. Such premium. Wow. 🐕🌙🎉

### Summary

v0.3.1 is a pure polish-and-UX release on top of the v0.3.0 feature foundation.
All requested features are now fully functional, the branding is consistent, and the app
has been elevated with micro-interactions, tooltips, accessibility improvements,
and more than a few fun surprises for the Doge faithful.

---

### Added

#### TX/RX Packet Direction Labels
- **ReceiveTab** now shows ALL LoRa traffic (both sent and received), not just incoming.
- Every packet row has a clear direction indicator:
  - **↑ green** — TX (sent by this node) with green left-border accent
  - **↓ blue**  — RX (received from remote node) with blue left-border accent
- TX/RX **filter toggle** (All / ↑ TX / ↓ RX) at the top of the tab — filterable live log!
- TX packets are emitted by Rust via a new `radio-packet-tx` Tauri event when `send_transaction` fires.
- TX/RX packet counts now tracked separately: `totalSent` + `totalReceived` in the radio store.

#### Dedicated "Show Address QR" Button in Wallet Tab
- **WalletTab** now has an explicit `📷 Show Address QR` toggle button on the address card.
- QR code panel slides in below the address field — completely separated from the private key section.
- Private key reveal is a separate, independent control — no accidental cross-reveal.
- QR panel includes its own Copy Address button for convenience.

#### Dynamic Signal Strength Descriptions
- **Dashboard** signal panel now shows a fun, dynamic description based on RSSI:
  - `≥ −50 dBm` → `🌟 Much Strong Radio!`
  - `≥ −65 dBm` → `🐕 Very Strong Signal`
  - `≥ −80 dBm` → `📡 Good Signal — Wow`
  - `≥ −95 dBm` → `〰️ Moderate Signal`
  - `≥ −110 dBm` → `😬 Weak Signal — Much Far`
  - `< −110 dBm` → `❌ Very Weak — Such Distance`

#### Tooltips Everywhere
Rich, informative, occasionally humorous `title` tooltips added to:
- **All Dashboard stat cards** — frequency, power, SF, bandwidth, packets sent/received
- **RSSI meter** — context-aware description based on signal level
- **SNR readout** — explanation of signal-to-noise ratio
- **Settings sliders** — TX power range, frequency input
- **All SF/BW/CR selector buttons** — per-option explanations with Doge flair
- **Node address fields** — per-byte explanation (region, community, node)
- **NavBar tabs** — what each tab does, why it's disabled when not connected
- **WalletTab buttons** — generate, copy, reveal, show QR
- **ReceiveTab filter buttons** — what each filter shows
- **Save to Device button** — different tooltip when connected vs. disconnected

#### Copy-to-Clipboard on All Fields
- **WalletTab**: address (×2: main row + QR panel), public key, private key
- **SettingsTab**: node address copy button
- **Dashboard**: copy button on every packet log entry
- **ReceiveTab**: copy button per packet (decoded or hex depending on toggle)
- **ConnectionPanel**: node address (existed in v0.3.0, now consistently styled)
- All copy buttons show `✅ Copied!` feedback for 1.5–2 s

#### Accessibility Improvements
- All interactive elements have `aria-label` attributes
- Toggle buttons use `aria-pressed` for screen reader state
- Filter groups use `role="group"` with `aria-label`
- Packet logs use `role="log"` with `aria-live="polite"`
- RSSI meter uses `role="meter"` with proper `aria-valuenow/min/max`
- Error and status messages use `role="alert"` / `role="status"` / `aria-live`
- All keyboard-focusable elements have a yellow `outline` focus ring (`focus-visible`)

#### Easter Egg: Konami Code 🎮
- Type ↑ ↑ ↓ ↓ ← → ← → B A anywhere in the app
- Triggers confetti + a random "much wow" toast popup
- One of four rotating messages: "Such secret! Very Konami. Much wow!"
- The most important feature in v0.3.1.

#### New CSS Animations
- `packet-arrive` — packets slide in from left with slight vertical movement
- `signal-pulse` — signal bars gently pulse green when signal is strong
- `verified-pop` — verified checkmark pops in with a satisfying scale animation
- `konami-rainbow` — entire app hue-rotates when Konami code is activated

### Changed

#### Branding Fix: "DOGE RADIO" → "RadioDoge"
- **NavBar** top-left title corrected from `DOGE RADIO` (old, inconsistent) to `RadioDoge`
- Consistent with the hero text, window title, and product name throughout

#### Auto-Reconnect Backoff Adjusted
- Initial backoff increased from 1 s to **5 s** — gives the OS time to fully re-enumerate the USB device after a re-plug (Windows needs ~3–4 s)
- Maximum backoff increased from 30 s to **60 s** — more patient for slow hardware
- Backoff schedule: 5 s → 10 s → 20 s → 40 s → 60 s (capped)
- Reset back to 5 s on successful reconnect (previously reset to 1 s)

#### ReceiveTab Becomes Combined TX/RX Log
- Renamed internal heading from "📥 Live Packet Monitor" to "📡 Live Packet Log"
- Now shows both TX (sent) and RX (received) packets in one scrollable, filterable view
- Packet entries have direction-colored left borders (green = TX, blue = RX)
- RSSI column hidden for TX packets (RSSI is meaningless for sent packets); shows "sent ↑" instead

#### Dashboard Live Packet Log
- Also shows TX + RX packets with direction arrows
- Per-entry copy button added
- RX/TX count badges added to the log header

#### Settings Tab Verification Message
- Success message now explicitly says **"Verified ✓"** when round-trip check passes
- Error message now says "device did not confirm within 1 s" with actionable advice

#### Radio Store Updated
- `PacketEntry` type added (extends `IncomingPacket` with `direction: 'TX' | 'RX'`)
- `addSentPacket()` function added for TX packet logging
- `totalSent` counter added alongside existing `totalReceived`
- `clearPackets()` now also resets `totalSent`

### Fixed

- NavBar tab `title` attribute now shows the full descriptive tooltip instead of just the label
- Settings node address fields now have per-field aria-labels and tooltips
- WalletTab QR code is now decoupled from the private key reveal — no more implicit coupling
- Dashboard stat cards now have `role="figure"` and `aria-label` for accessibility
- `[title]` CSS rule added to `app.css` ensuring `cursor: help` on all tooltipped elements
- Button `[title]` overrides to `cursor: pointer` so buttons feel correct

### Version Bumps

| File | Before | After |
|------|--------|-------|
| `radiodoge-gui/package.json` | `0.3.0` | `0.3.1` |
| `radiodoge-gui/src-tauri/tauri.conf.json` | `0.3.0` | `0.3.1` |
| `radiodoge-gui/src-tauri/Cargo.toml` | `0.3.0` | `0.3.1` |
| `crates/radiodoge-core/Cargo.toml` | `0.3.0` | `0.3.1` |
| `crates/radiodoge-cli/Cargo.toml` | `0.3.0` | `0.3.1` |
| `+page.svelte` footer | `v0.3.0` | `v0.3.1` |

---

## [0.3.0] — 2026-03-07 — 🚀 Full v0.3.0 Feature Release

> **Real hardware integration complete. Much functionality. Very radio. Such wow. 🐕🌙**

### Added

#### App Icon
- **Real PNG icon**: `Radio_Doge.png` (JPEG-disguised-as-PNG) has been converted to a genuine
  PNG using Python Pillow; all icon targets (`icons/doge-radio.png`, `icons/icon.ico`,
  `images/Radio_Doge.png`, `static/doge-radio.png`) are now valid PNG files with correct magic bytes.
  The Doge boombox image is now the official Windows application icon.

#### Firmware Version Badge
- On connect, RadioDoge sends `CMD_GET_FIRMWARE_VERSION` (0x20) to the device and waits up to
  500 ms for a response. The firmware version string (e.g. `v1.2.3`) is displayed as a badge
  in the Connected status card. If the device doesn't respond, shows `FW v?.?.?` gracefully.

#### Auto-Reconnect on USB Re-Plug
- A background watchdog task monitors connection health after each successful connect.
- On unexpected disconnect, emits `reconnecting` status and retries the connection with
  exponential backoff: 1 s → 2 s → 4 s → … → 30 s max.
- The user can stop the watchdog at any time by clicking Disconnect.
- UI shows "🔄 Reconnecting..." spinner during reconnect attempts.

#### LoRa Settings Round-Trip Verification
- `update_lora_settings` now performs a round-trip check after sending `CMD_SET_NODE_ADDR`:
  it queries the device with `CMD_GET_NODE_ADDR` and waits up to 1 second for a response
  confirming the new address.
- Returns `true` (verified) or `false` (sent but no confirmation) to the frontend.
- SettingsTab now shows `✅ Settings saved & verified on device!` or
  `⚠️ Settings sent — could not verify on device`.

#### Desktop Toast Notifications for DOGE TX
- Incoming `CMD_DOGE_TX` packets trigger a native OS desktop notification via
  `tauri-plugin-notification`, showing the decoded amount and recipient address.
- Works on Windows, macOS, and Linux without any additional setup.

#### Clickable Driver Download Links
- The "No ports detected" help box in ConnectionPanel now has clickable buttons
  (`🔗 silabs.com — CP210x USB Driver` and `🔗 wch-ic.com — CH340 USB Driver`)
  that open the driver download pages in the system browser via `@tauri-apps/plugin-shell`.

#### Copy-to-Clipboard
- **ConnectionPanel**: copy button (📋) next to Node Address in the connected status card.
- **ReceiveTab**: copy button per packet row — copies the decoded text or raw hex depending
  on the current "Show raw hex" toggle. Shows ✅ briefly on copy.

### Changed

- `update_lora_settings` Tauri command return type: `Result<(), String>` → `Result<bool, String>`
  (boolean indicates round-trip verification result; `false` when not connected = stored locally).
- `ConnectionStatusEvent` struct now has a `firmware_version: Option<String>` field.
- `ConnectionStatusType` now includes `'reconnecting'` status.
- Version bumped to `0.3.0` across all crates, `tauri.conf.json`, and `package.json`.
- Footer version string updated to `RadioDoge v0.3.0`.

### Fixed

- Node address auto-update from read loop: the serial read loop now correctly updates
  `node_address` in `SerialManager` when a `CMD_GET_NODE_ADDR` (0x00) response is received,
  rather than always returning the default `10.0.1`.

---

## [0.2.4] — 2026-03-07 — 🔧 Build Fix: Bad Icon Removed, Versions Stabilised

> **MSI was broken by a JPEG-disguised-as-PNG icon. Reverted to working defaults.**
> Much relief. Very fix. Such build. Wow. 🐕

### Fixed — CI Build: "Invalid PNG signature" Error

**Root cause**: `Radio_Doge.png` in the repository root is a **JPEG file with a `.png` extension**
(starts with `\xFF\xD8\xFF` JPEG magic bytes, not `\x89PNG\r\n\x1a\n`).
When it was copied to `radiodoge-gui/src-tauri/icons/doge-radio.png` and added to
`tauri.conf.json`'s `bundle.icon` list, Tauri's bundler (which uses the Rust `image` crate)
tried to parse it as a PNG during the MSI build and threw:

```
failed to read icon .../doge-radio.png: Invalid PNG signature.
```

The same JPEG was referenced as the `trayIcon.iconPath`, which also fails validation.

**Fix applied**:

| File | Change |
|---|---|
| `radiodoge-gui/src-tauri/tauri.conf.json` | Removed `icons/doge-radio.png` from `bundle.icon`; reverted `trayIcon.iconPath` to `icons/icon.png` |
| All `Cargo.toml` files + `package.json` | Hard-set to `0.2.4` (version had drifted to `0.3.0`) |
| `tauri.conf.json` window title | Removed "v0.3.0" from title string |
| `ConnectionPanel.svelte` / `NavBar.svelte` | Removed hardcoded "v0.3.0" from UI labels |
| `crates/radiodoge-core/src/wallet.rs` | Removed unused `Context` import (`anyhow::{Context, Result}` → `anyhow::Result`) |
| `radiodoge-gui/src-tauri/src/lib.rs` | Removed unused `Runtime` import from `use tauri::{...}` |

The `doge-radio.png` and `images/Radio_Doge.png` files are **not deleted** — they remain in the
repo. The frontend still serves `/doge-radio.png` (browsers are lenient with MIME types), so
the hero image and NavBar logo continue to display correctly in the app.

**To fully fix the icon in a future release**: convert `Radio_Doge.png` to a real PNG
(e.g. with `magick Radio_Doge.png -format png Radio_Doge_fixed.png` in ImageMagick),
replace the file, and re-add it to the `bundle.icon` list.

### Note — Feature Work Preserved

All serial port detection improvements (CP210x/CH340 USB detection, `list_ports_with_info`,
Ping Device button, driver error hints) and the Development Roadmap in README.md remain intact.
Only icon references and version numbers were changed in this commit.

---

## [0.3.0-dev] — 2026-03-07 — 🚀 Doge Radio Image + Heltec Connection Polish (reverted to 0.2.4)

> **Features shipped in this cycle — icon was broken, version reverted, features kept.**
> Much image. Very hero. Such radio. Wow. 🐕📻🌙

### Added — Legendary Doge Radio Hero Image

- **`images/Radio_Doge.png`** — canonical 512×512 RGBA PNG home for the epic Doge/boombox artwork
- **`radiodoge-gui/src-tauri/icons/doge-radio.png`** — Tauri bundle icon (app icon in taskbar, MSI, system tray)
- **`radiodoge-gui/static/doge-radio.png`** — served as `/doge-radio.png` by the Vite frontend for all in-app uses

### Changed — UI: Hero Image Activated Everywhere

| Location | Before | After |
|---|---|---|
| **Connect tab hero** | Bouncing 🐕 emoji | 180×180 `doge-radio.png` with gold drop-shadow glow, `bounce-doge` animation |
| **Dashboard empty state** | 📡 emoji | 160×160 `doge-radio.png` with subtle opacity + glow when disconnected |
| **NavBar logo** | 🐕 emoji | 32×32 `doge-radio.png` with rounded corners + gold drop-shadow |
| **NavBar subtitle** | `WIRELESS P2P` | `v0.3.0 · WIRELESS P2P` |
| **Connect tab title** | `RadioDoge` | `RadioDoge v0.3.0` |
| **Window title** | `RadioDoge 🐕 — Wireless Dogecoin` | `RadioDoge v0.3.0 🐕 — Wireless Dogecoin` |
| **Tray icon** | `icons/icon.png` | `icons/doge-radio.png` |
| **Bundle icon list** | Standard generated icons only | `doge-radio.png` added as primary 512×512 source |
| **Copyright** | `© 2024` | `© 2026` |

### Added — Smart USB Serial Port Detection

**`crates/radiodoge-core/src/serial.rs`** — new `list_ports_with_info()` method:

- Inspects each port's `SerialPortType` to identify USB serial adapters (vs. native COM ports)
- Detects known Heltec/ESP32 USB adapters by VID/PID:
  - Silicon Labs **CP210x** (VID `0x10C4` / PID `0xEA60`) — most common on Heltec V2
  - Jiangsu Qinheng **CH340** (VID `0x1A86` / PID `0x7523`) — common ESP32 breakout boards
  - **CH9102** (VID `0x1A86` / PID `0x55D4`) — some Heltec V3 boards
  - Espressif **native USB-CDC** (VID `0x303A`) — ESP32-S2/S3/C3 built-in USB
  - **FTDI** (VID `0x0403`) — less common but used on some Heltec variants
- Ports returned in priority order: likely-Heltec first → other USB → native COM/PCI
- Returns `Vec<PortInfo>` with `name`, `isUsb`, `manufacturer`, `product`, `vid`, `pid`, `description`, `isLikelyHeltec` fields
- `list_ports()` now calls `list_ports_with_info()` internally (backward compatible — same sort order)

**`crates/radiodoge-core/src/types.rs`** — new `PortInfo` struct (Serialize/Deserialize, camelCase).

### Added — `list_ports_detailed` Tauri Command

**`radiodoge-gui/src-tauri/src/lib.rs`** — exposes the rich port info to the Svelte frontend:

```ts
// invoke returns PortInfo[]
const ports = await invoke<PortInfo[]>('list_ports_detailed');
```

### Added — Actionable Error Messages on Connection Failure

`connect_port` now maps common Windows serial errors to human-readable hints:

| OS Error | User-Facing Message |
|---|---|
| `Access is denied` | "Another app (Arduino IDE, PuTTY) has this port open — close it first" |
| `could not open` / `No such file` | Lists CP210x and CH340 driver download URLs |
| `device has been removed` | "Check USB cable — some USB-C cables are power-only" |

### Added — Ping Device Button in Connection Panel

**`radiodoge-gui/src/lib/components/ConnectionPanel.svelte`**:

- **🟢 / 🔵 / ⚪ port badges** in the dropdown — `🟢 Heltec`, `🔵 USB`, `⚪ COM`
- **Auto-selects** the most likely Heltec port on refresh (likely-Heltec → USB → first)
- **Product name** displayed next to the selected port (e.g. `CP2102 USB to UART Bridge`)
- **📡 Ping Device** button (visible when connected) — sends a PING and shows round-trip latency in ms
- **Driver hint box** with ordered steps + CP210x/CH340 download URLs when no ports are found
- **Warning** when user selects a non-USB port (likely not their Heltec)
- Ping success/failure feedback with coloured result pill

### Added — Development Roadmap in README.md

New `🗺️ Development Roadmap` section with four phases:

- **v0.2.x** (✅ Complete) — Foundation, with sub-point status for all shipped items
- **v0.3.x** (🔄 Current) — Heltec polish + UX improvements, with near-term `🔜` sub-items
- **v0.4.x** — Full DOGE tx over LoRa: UTXO fetch, HD wallet, SPV, address book, QR scan, fee estimation
- **v0.5.x** — Meshtastic interop: protobuf encoding, bridge mode, multi-channel scan, GPS, store-and-forward
- **Future** — Android, iOS, WASM inspector, gateway dashboard, Lightning over LoRa

All existing serial communication, wallet generation, transaction signing, and LoRa packet logic remain fully functional (shipped in v0.2.4).

### Changed — Version bumped to 0.3.0

- `radiodoge-gui/package.json`
- `radiodoge-gui/src-tauri/tauri.conf.json`
- `radiodoge-gui/src-tauri/Cargo.toml`
- `crates/radiodoge-core/Cargo.toml`
- `crates/radiodoge-cli/Cargo.toml`

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
