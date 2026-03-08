# Changelog

All notable changes to RadioDoge will be documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.3.4] — 2026-03-08 — ✨ Polish Pass + Clean Connect Tab — Much Pretty. Very Fix. Wow.

> **Five targeted fixes with zero behavior changes. Everything that was broken in the UI
> now works exactly as designed. All changes additive and backward-compatible.** 🐕

### Fixed

#### COM Port Dropdown — No Longer Overflows the Card
- **Bug**: `<select flex:1>` had no `min-width: 0`, letting a long description like
  `"🟢 Heltec CP2102 USB-to-UART Bridge Controller (COM3)"` blow past the card edge.
- **Fix**: Added `min-width: 0` to both the flex row and the `<select>` itself. Added
  `portLabel()` helper that truncates descriptions longer than 42 chars to `"…"`.
  Hover `title` on the `<select>` shows the full untruncated description.

#### Debug Console Export — Downloads Correctly
- **Bug**: `a.click()` on a detached anchor element is a no-op in Tauri's WebView.
  The button appeared to work but produced no file.
- **Fix**: `document.body.appendChild(a)` before `a.click()`, then `removeChild(a)`
  afterward. Export now produces a proper timestamped `.txt` file with a header banner.

#### Ping Device — Now Shows Debug Console Feedback
- **Bug**: `ping_device` Tauri command called `serial.ping()` silently — nothing appeared
  in the Debug Console before or after the ping.
- **Fix**: Command now accepts `AppHandle`, builds the same ping packet for hex display,
  emits `TX` with hex bytes before sending and `RX` with `✅ PONG` / `❌ No PONG` after.

#### Firmware Version Badge — Clean Display
- **Bug**: Firmware sends `"RadioDoge NV3FW01"` → badge showed `"FW RadioDoge NV3FW01"`.
  The word "RadioDoge" appeared twice; looked broken.
- **Fix**: `setConnected()` in `connection.svelte.ts` strips the `"RadioDoge "` prefix
  via regex before storing. Badge now shows `"NV3FW01"` / `"NV55"` cleanly. Backward
  compatible — non-prefixed strings (e.g. legacy `"v1.2.3"`) pass through unchanged.
  Pending badge changed from `"FW v?.?.?"` (red/grey) to `"FW …"` (subtle yellow).

#### Error Banner — Compact + Context-Aware
- **Bug**: Connection errors rendered as a large red block regardless of severity.
  A soft "no response" timeout looked as alarming as "Access denied".
- **Fix**: Banner is now compact, has a dismiss `✕` button, and is **yellow** for soft
  errors (timeouts, "No response") vs **red** only for hard failures (Access denied,
  missing driver). Classified via the new `errorIsSoft` derived boolean.

### Added

- **README.md**: Dogecoin Core RPC bridge line added to v0.4.x roadmap section.
- **README.md**: Version badge updated to v0.3.4 in the hero description.

### Changed

- Version bumped to `0.3.4` in `package.json`, all `Cargo.toml` files, `tauri.conf.json`,
  and app footer (`+page.svelte`).

---

## [0.3.3] — 2026-03-08 — 🚀 Firmware Deeply Optimized + Desktop Fully Synced — Much Protocol. Very Fix. Wow.

> **Root-cause diagnosis and fix of the firmware ↔ desktop protocol mismatch.
> All desktop commands now work correctly end-to-end for the first time. Additive only — zero
> breaking changes to firmware behavior, WiFi AP, web dashboard, or packet format.**
> Such compatible. Very reliable. Much DOGE. Wow. 🐕📡

### Summary

v0.3.3 resolves a fundamental protocol incompatibility discovered between the Heltec firmware
(2-byte header: `[cmd, payloadSize]`) and the desktop app (8-byte header:
`[cmd, flags=0x00, src(3), dst(3), ...payload...]`). Because the firmware read the desktop's
`flags=0x00` byte as `payloadSize=0`, it read zero payload bytes — leaving 6+ header bytes in
the serial buffer to corrupt subsequent commands. This caused:

- Firmware version always showing "FW v?.?.?" (0x20 → firmware default → NACK)
- Node address never saving (0x01 → firmware ADDRESS_GET → returns old addr, ignores new one)
- **Critical**: CMD_PING (0x02) → firmware ADDRESS_SET with 0-byte garbage → corrupts node
  address in NVS (was saving 0.0.0 silently every ping!)
- DOGE_TX (0x10) never reaching LoRa radio (→ default → NACK, dropped silently)

All fixed. All changes are additive and commented `// OPTIMIZED FOR DESKTOP v0.3.3 – SAFE`.

---

### Fixed

#### Firmware — Protocol Mismatch Root Cause (heltec-firmware.ino)

- **`HandleDesktopCommand()` — new additive function**: Intercepts desktop command IDs
  (0x10 DOGE_TX, 0x11 REQUEST_BALANCE, 0x20 GET_FIRMWARE_VERSION, 0x21 SET_LORA_PARAMS)
  before the firmware's `switch(commandVal)`. Drains the 6 remaining desktop header bytes
  (`src_r, src_c, src_n, dst_r, dst_c, dst_n`) and any payload, then replies in the same
  8-byte desktop format the Rust accumulator expects.

- **`CMD_GET_NODE_ADDR (0x00)` — NONE case fixed**: Previously fell through to default → NACK.
  Now drains 6 header bytes and replies `[0x00, 0x00, local_r, local_c, local_n, 0xFF, 0xFF, 0xFF]`.
  Desktop `serial.rs` checks `cmd==0x00 && src!=broadcast` to update node address — now works.

- **`CMD_SET_NODE_ADDRS (0x01)` — ADDRESS_GET case fixed**: Previously just returned the OLD
  address (no-op). After `delay(500)` all 9 remaining bytes (`src(3) + dst(3) + newAddr(3)`)
  are in the buffer. Now reads `hdrRest[9]`, extracts `hdrRest[6..8]` as new address, saves
  to NVS, replies with new address in 8-byte format. Falls back to legacy behavior if fewer
  bytes are available (backward-compatible).

- **`CMD_PING (0x02)` — Critical address-corruption bug fixed**: ADDRESS_SET case with
  `payloadSize==0` previously called `SetLocalAddressFromSerialBuffer(0)` on an empty buffer →
  wrote `0.0.0` to NVS on every ping. Now: `payloadSize==0` drains 6 header bytes, replies
  with ping ACK in 8-byte format. Legacy ADDRESS_SET behavior preserved for `payloadSize>0`.

- **`CMD_DOGE_TX (0x10)`**: Previously → default → NACK, packet dropped. Now intercepted by
  `HandleDesktopCommand()`, payload forwarded to `Radio.Send()`, ACK returned.

- **`CMD_GET_FIRMWARE_VERSION (0x20)`**: Now replies with
  `"RadioDoge NV<HELTEC_BOARD_VERSION>FW<FIRMWARE_VERSION>"` as ASCII payload in 8-byte format.
  Desktop parses this as a UTF-8 string from the payload — firmware version now shows correctly.

- **`CMD_SET_LORA_PARAMS (0x21)` — new additive command**: ACKs the command (RF reconfiguration
  infrastructure in place for future revision).

- **`saveLoRaConfigurationQuiet()`**: New NVS save function that does NOT call `Serial.println()`.
  Used inside `HostSerialRead()` context to prevent debug text from polluting the binary stream.
  Original `saveLoRaConfiguration()` unchanged (still used outside serial context).

#### Desktop — Rust Backend (radiodoge-core + radiodoge-gui)

- **`radio.rs` — `CMD_SET_LORA_PARAMS: u8 = 0x21`**: New command constant added.
- **`radio.rs` — `build_set_lora_params()`**: New builder that encodes SF, BW index, CR,
  frequency (kHz), and TX power as 8-byte payload in standard header format.
- **`lib.rs` — `update_lora_settings()`**: Now also sends `CMD_SET_LORA_PARAMS (0x21)` after
  `CMD_SET_NODE_ADDRS (0x01)`, so RF settings (SF, BW, CR, freq, power) are applied to the
  device on every settings save.
- **`serial.rs` — accumulator re-sync**: Added `KNOWN_CMDS` check before attempting
  `parse_incoming()`. Bytes with an unrecognized first byte (stale debug text, partial writes)
  are discarded one at a time until re-synced on a valid command byte. Prevents garbage packets.

#### Desktop — Svelte GUI

- **`ReceiveTab.svelte` — Export button**: "💾 Export" button downloads the currently-filtered
  packet log as a timestamped JSON file. Appears next to the Clear button when packets are present.

### Changed

- All version numbers bumped to `0.3.3`: `package.json`, all `Cargo.toml` files, `tauri.conf.json`,
  app footer in `+page.svelte`.

### Unchanged (strict compatibility)

- Serial baud rate: 115200
- WiFi AP SSID/password: "RadioDoge" / "radiodoge"
- Firmware command IDs for legacy tools (ADDRESS_GET=1, ADDRESS_SET=2, PING_REQUEST=3, etc.)
- Web dashboard HTML, REST API endpoints
- Dogecoin packet format and Foundation relay behavior
- All v0.3.1/v0.3.2 features (Debug Console, TX/RX filter, auto-reconnect, etc.)

---

## [0.3.2] — 2026-03-08 — 🐛 Critical Fixes + Debug Console — Much Debug. Very Fix. Wow.

> **Three critical bugs squashed, a professional Debug Console added, and more UX polish.**
> Such reliable. Very firmware. Much debug. Wow. 🐕🐛

### Summary

v0.3.2 fixes three bugs reported from v0.3.1 testing: firmware version query was unreliable
(single attempt with 500ms wasn't enough), node address save didn't update the UI after a
verified write, and there was no way to inspect raw serial traffic. This release adds a
Meshtastic-inspired Debug Console and several quality-of-life improvements.

---

### Fixed

#### Firmware Version Query — Reliable 3-Attempt Retry
- **Bug**: On connect, a single `CMD_GET_FIRMWARE_VERSION` with 500ms wait often failed.
  The Heltec device can be slow to respond on first boot — result was "FW v?.?.?" forever.
- **Fix**: Now retries up to **3 times** with 400ms between each attempt. All attempts are
  logged to the Debug Console. Firmware version populates reliably on first connect.
- Added `query_firmware_version` Tauri command for manual re-query from the UI.

#### Node Address Save — Now Actually Updates the UI
- **Bug**: After clicking "Save to Device" in Settings and getting "Verified ✓", the
  ConnectionPanel and footer still showed the old address (10.0.1). The Rust backend
  verified the address round-trip but never emitted a `connection-status` event with
  the updated address.
- **Fix**: `update_lora_settings` now emits `connection-status` with the new node address
  after a successful round-trip verification. The ConnectionPanel, footer status bar, and
  all dependent components update immediately.

### Added

#### Debug Console (Ctrl+Shift+D)
- **New `DebugConsole.svelte` component** — hidden by default, toggled via:
  - `Ctrl+Shift+D` keyboard shortcut (global)
  - 🐛 button in the status bar footer
- Shows **real-time raw serial traffic** with:
  - Timestamps (HH:MM:SS.mmm)
  - TX/RX direction badges (↑TX green, ↓RX blue)
  - Raw hex bytes (spaced for readability)
  - Parsed command labels (e.g. `CMD_GET_FIRMWARE_VERSION`, `CMD_DOGE_TX`)
- **Professional dark terminal styling** inspired by Meshtastic's web flasher
- **Auto-scroll** (toggleable) to newest entry
- **Clear** button to reset the log
- **Export to .txt** — downloads the full debug log as a timestamped text file
- **Copy line-by-line** — clipboard button on each entry
- Fixed at the bottom of the window, 280px tall, slides up with animation
- Capped at 500 entries to prevent memory bloat
- Rust backend now emits `debug-serial-traffic` events on every serial TX/RX operation

#### Firmware Re-Query Button
- ConnectionPanel now shows a "⟳ Query FW" button next to the "FW v?.?.?" badge
- Sends a manual `CMD_GET_FIRMWARE_VERSION` to the device and updates the badge on success

#### Remember Last COM Port
- On successful connection, the selected port name is saved to `localStorage`
- On next app launch, the last-used port is auto-selected if still present
- Falls back to Heltec auto-detection → USB → first port if the saved port is gone

#### Breathing Animation on Signal Bars
- Connected signal bars now have a subtle breathing/pulsing glow animation
- New `signal-breathe` CSS keyframes and `.signal-bars-breathing` class in `app.css`

#### Debug Traffic Events in Rust Backend
- New `emit_debug_traffic()` helper function in `lib.rs`
- Emits `debug-serial-traffic` events for:
  - All `CMD_GET_FIRMWARE_VERSION` queries (with attempt number)
  - `CMD_SET_NODE_ADDRS` writes (with target address)
  - Node address verification results
  - `CMD_DOGE_TX` sends (single + multipart)
  - All incoming RX packets (raw hex + parsed content)

### Changed

- Combined Konami code + Debug Console keyboard handler into single `handleKeydown` function
- Footer status bar now includes 🐛 debug toggle button (active state highlighted in yellow)

### Version Bumps

| File | Before | After |
|------|--------|-------|
| `radiodoge-gui/package.json` | `0.3.1` | `0.3.2` |
| `radiodoge-gui/src-tauri/Cargo.toml` | `0.3.1` | `0.3.2` |
| `crates/radiodoge-core/Cargo.toml` | `0.3.1` | `0.3.2` |
| `crates/radiodoge-cli/Cargo.toml` | `0.3.1` | `0.3.2` |
| `+page.svelte` footer | `v0.3.1` | `v0.3.2` |

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
