# Changelog

All notable changes to RadioDoge will be documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.3.16] — 2026-04-15 — 🐕 Android Settings + BLE Toggle + Live Log Polish — Much Polish. Very Stable. Wow.

> **Settings tab actions now work on Android. Start Gateway double-press fixed on desktop. Live Packet Log fully readable on 360dp phones. Debug export uses native share sheet on Android. BLE advertising toggle added. Board MAC read fixed for both platforms.**
> Such cross-platform. Very consistent. Much UX. Wow. 🐕📡

### Fixed

#### Settings tab actions failing on Android ("Not connected to board")
- **Root cause**: All Rust commands (`set_gateway_mode`, `set_wifi_enabled`, `query_mac`) checked `state.serial.is_connected()` which is always `false` on Android (the JS bridge owns the USB port, not the Rust `SerialManager`). Every Settings tab action immediately returned an error.
- **Fix**: Added four new `mobile_build_*` Tauri commands that return raw packet bytes for the JS bridge to write:
  - `mobile_build_set_gateway(enable: bool)` — builds a `CMD_SET_GATEWAY` packet
  - `mobile_build_wifi_toggle(enable: bool)` — builds a `CMD_WIFI_TOGGLE` packet
  - `mobile_build_ble_toggle(enable: bool)` — builds a `CMD_BLE_TOGGLE` packet
  - `mobile_build_get_mac()` — builds a `CMD_GET_MAC` packet
- **`SettingsTab.svelte`** detects Android via `bridge.isAndroid()` and takes the mobile path (invoke → get bytes → `bridge.mobileSendBytes(bytes)` → optimistic state update) instead of the Rust-serial path.
- **`mobileSendBytes`** — new exported function in `connection-bridge.ts` that routes to `_bleWrite` or `_usbWrite` depending on the active transport.

#### Start Gateway double-press required on desktop
- **Root cause**: `invoke('set_gateway_mode')` returned before the async `board-sync` Tauri event was processed by JS, so `connection.gatewayMode` still showed the old value when the button re-rendered, requiring a second press to visually confirm.
- **Fix**: `set_gateway_mode` already returns the confirmed `bool` from the board. JS now uses it directly (`connection.gatewayMode = confirmed`) instead of waiting for the `board-sync` event.

#### Read MAC failing on desktop (timeout too tight)
- **Root cause**: `query_mac` sent a `CMD_GET_MAC` packet and waited 400ms for the board response before reading the cache. Boards with a slow UART or queue delay missed the window.
- **Fix**: Timeout extended to 600ms.

#### Read MAC failing on Android
- **Root cause**: `query_mac` had no mobile path. It sent the packet via `state.serial.send()` (no-op on Android) and waited, always returning `None`.
- **Fix**: `mobile_push_bytes` now handles `CMD_GET_MAC` responses: parses the 6-byte MAC, caches it via `state.serial.update_board_mac()`, and emits a `mobile-board-mac` Tauri event. `SettingsTab.svelte` listens for this event and updates `connection.boardMac`. The Rust `query_mac` command also returns the cached value immediately on mobile (when `current_port` is set but `serial.is_connected()` is false).

#### Live Packet Log text cut off / overlapping on Android phones
- **Root cause**: All packet metadata (direction, timestamp, command, source, content, RSSI, copy button) was in a single `display: flex` row with `gap: 8px`. On 360dp phones the total content far exceeded the viewport width; content was clipped or pushed off-screen.
- **Fix**: Restructured each packet entry into two rows:
  - **Row 1** (`flex-wrap: nowrap`): direction arrow, timestamp, command badge, source address — all `flex-shrink: 0` with `white-space: nowrap` — then a spacer, RSSI, and copy button pinned to the right.
  - **Row 2**: packet content with `word-break: break-all; overflow-wrap: anywhere` so long hex strings and decoded text wrap freely.
  - Font size: `clamp(0.65rem, 2vw, 0.76rem)` — scales with viewport width on phones, caps at desktop readable size.
- **Further**: Log container now has `overflow-x: hidden; min-width: 0` and each packet entry has `min-width: 0; overflow: hidden` so the card never generates a horizontal scrollbar on any viewport.

#### Debug log export unreliable on Android
- **Root cause**: The `<a download="...">` anchor click approach is not reliably honoured by Tauri's Android WebView — the file was silently discarded with no user-visible feedback or file manager entry.
- **Fix**: `exportToTxt` now checks for `navigator.share` + `navigator.canShare({ files: [...] })` (both present on Android). When available, it calls `navigator.share({ files: [file], title: 'RadioDoge Debug Log' })` which triggers the native Android share sheet (user can choose Files, Gmail, etc.). On desktop / WebView without share-file support, the existing anchor download fallback runs unchanged.

#### Android home screen showing default Tauri icon
- **Root cause**: `tauri android init` uses `icons/icon.png` (64×64 default Tauri placeholder) as the source for Android launcher icon generation. The placeholder was the original icon generated when the project was initialized and was never replaced with the RadioDoge image.
- **Fix**: Added a `tauri icon src-tauri/icons/doge-radio.png` step in `build-android.yml` **before** `tauri android init`. The `tauri icon` command re-generates all platform icon files — including `icon.png` at the correct size — from the 1024×1024 RadioDoge source. When `tauri android init` then runs, it generates the Android mipmap assets (mdpi / hdpi / xhdpi / xxhdpi / xxxhdpi) from the RadioDoge image.

#### BLE GATT connection silently producing no data (board "connected" but unresponsive)
- **Root cause 1 — CCCD timing**: Some Android BLE stacks silently drop the NOTIFY descriptor write when it arrives within ~200 ms of the GATT Connected callback. `blec.connect()` succeeds and `onReceiveData()` returns without error, but notifications are never actually enabled — the board can receive commands but the app never hears back.
- **Fix**: Added a 250 ms delay in `_connectBluetoothAndroid()` between `blec.connect()` and `blec.onReceiveData()` to let the GATT connection stabilise before enabling notifications.
- **Root cause 2 — sendData byte type**: Some `@mnlphlp/plugin-blec@0.4.x` builds expect `number[]` rather than `Uint8Array` in the Tauri IPC serialisation. Passing `Uint8Array` directly could silently send zero bytes.
- **Fix**: `_bleWrite()` now passes `Array.from(data)` to `blec.sendData()`.
- **Root cause 3 — silent failure**: `onReceiveData()` subscribe failure was previously caught and only logged as a warning, leaving the connection in a state where the app appeared connected but the board was deaf.
- **Fix**: `onReceiveData` failure now throws, propagates to `mobileConnect()` as a visible error, disconnects, and clears the BLE address so the user can retry.

#### BLE disconnect-during-stabilisation race
- **Root cause**: After the 250 ms `_sleep()` in `_connectBluetoothAndroid()`, if the user called `disconnect()` during the delay, `activeBleAddress` was already cleared — but `onReceiveData()` was still called against the now-closed GATT connection, silently failing or throwing.
- **Fix**: Added a guard immediately after the sleep: `if (!activeBleAddress) return;` — connection attempt aborts cleanly if a disconnect raced the delay.

#### Ping timeout error message incorrect (said "500 ms", actual timeout is 2 s on Android)
- **Root cause**: The mobile ping path in `ConnectionPanel.svelte` used a 2000 ms timeout but the error string read `"No response within 500 ms."` — copied from the desktop path which genuinely times out at 500 ms via the Rust serial layer.
- **Fix**: Message corrected to `"No response within 2 s."` for the Android path only; desktop message unchanged.

#### BLE toggle state not reset on disconnect
- **Root cause 1 — `ConnectionPanel.svelte`**: `panelBleEnabled` and `panelBleError` were not cleared in `mobileDisconnect()`. After disconnect+reconnect, the toggle showed the last-used state instead of the board's default (enabled).
- **Fix**: Added `panelBleEnabled = true; panelBleError = null;` to `mobileDisconnect()`.
- **Root cause 2 — `SettingsTab.svelte`**: Same issue for `bleAdvertisingEnabled` and `bleError` — no disconnect handler existed.
- **Fix**: Added a `$effect` that resets both to default (`true` / `null`) whenever `connection.isConnected` becomes `false`.

#### `fetchMac()` race on Android — wrong code path taken on fast connect
- **Root cause**: `isAndroidPlatform` was initialised as `$state(false)`. If `fetchMac()` fired before the `bridge.isAndroid()` Promise resolved (possible on fast connects), the desktop branch (`invoke('query_mac')`) ran on Android — a no-op at best, broken at worst.
- **Fix**: Changed initial value to `$state<boolean | null>(null)`. `fetchMac()` returns early if `isAndroidPlatform === null`, and the auto-fetch `$effect` is gated on `isAndroidPlatform !== null`. Platform detection resolves in the same microtask queue, so the fetch fires on the very next effect re-run with the correct branch.

#### Debug Console export: File constructor outside try-catch + AbortError bypassed fallback
- **Root cause 1**: `new File([blob], filename)` was constructed before the try-catch block in `exportToTxt()`. If the `File` constructor threw in an exotic WebView, the exception propagated uncaught and the entire export silently failed.
- **Root cause 2**: On `AbortError` (user cancelled the native share sheet), the function did an early `return` — bypassing the anchor-download fallback. On desktop Chrome/Firefox where `navigator.share` exists, cancelling the sheet meant no file was saved at all.
- **Fix**: Moved `File` construction inside the try block. Removed the early `return` on `AbortError` so the anchor download always runs as a fallback when sharing is unavailable or cancelled.

#### `commandName(0x28)` returning generic "CMD(0x28)" in Live Packet Log
- **Root cause**: `0x28` (CMD_BLE_TOGGLE) was missing from the `commandName()` map in `types/index.ts`.
- **Fix**: Added `0x28: 'BLE TOGGLE'` to the map.

#### Debug Console showing "CMD_?" for BLE toggle packets
- **Root cause**: `0x28` was missing from the `match` arm in `mobile_emit_debug_tx` in `lib.rs`.
- **Fix**: Added `0x28 => "CMD_BLE_TOGGLE"`.

#### Live Packet Log too tall on short phones (keyboard-open state)
- **Root cause**: Log container height was hardcoded at `300px`. On a ~600 px-tall viewport (keyboard open on a small Android phone), the packet log occupied half the screen.
- **Fix**: Changed to `height: clamp(180px, 38vh, 320px)` — scales proportionally with viewport height, floors at 180px (readable), caps at 320px on large screens.

#### BLE user-initiated disconnect triggering double `setDisconnected()` + duplicate Rust invocations
- **Root cause**: `_disconnectBluetoothAndroid()` called `blec.disconnect()` while `activeBleAddress` was still set. This triggered the `onDisconnect` callback registered in `blec.connect()`, which called `setDisconnected()` and `invoke('mobile_ble_disconnect')`. Then `_disconnectBluetoothAndroid` called both again (since `notifyRust=true`), producing a double-disconnect and two duplicate Rust invocations per clean disconnect.
- **Fix A**: In `_disconnectBluetoothAndroid`, `activeBleAddress` is now cleared to `null` *before* calling `blec.disconnect()`. The callback checks `activeBleAddress` on entry and returns immediately when null — making it a pure "unexpected board-side disconnect" handler.
- **Fix B**: Added `if (!activeBleAddress) return;` guard at the start of the `onDisconnect` callback.

#### `mobileRefreshing` spinner persists after switching USB↔BLE tabs
- **Root cause**: The USB/BLE tab-switch `onclick` handler cleared `selectedMobileDevice` and `mobileDevices` but did not reset `mobileRefreshing`. If a 5-second BLE scan was in progress when the user tapped the USB tab, the USB tab displayed the spinning "Scanning BLE… (5 s)" label and the Scan button remained disabled until the scan finished.
- **Fix**: Added `mobileRefreshing = false;` to the tab-switch handler.

#### `WalletTab` `load_saved_wallet` overwrites in-memory wallet on tab return
- **Root cause**: The `$effect` that calls `invoke('load_saved_wallet')` on mount ran on every mount. Since `{#if activeTab === 'wallet'}` unmounts WalletTab when the user navigates away, navigating back always re-ran the effect — overwriting any wallet the user had generated or imported in memory since the last disk save.
- **Fix**: Changed `$effect` to `onMount` so the load runs exactly once per component lifetime.

#### `wifiError` and `gatewayError` not cleared on disconnect
- **Root cause**: The existing `$effect` that resets `bleAdvertisingEnabled` and `bleError` on disconnect did not include `wifiError` or `gatewayError`. After disconnecting, the Settings tab could show a stale red error from the previous session.
- **Fix**: Added `wifiError = null; gatewayError = null;` to the disconnect `$effect`.

#### "Read MAC" button clickable while platform detection is unresolved
- **Root cause**: `fetchMac()` returns early silently when `isAndroidPlatform === null`. If the user tapped the button in the brief window before `bridge.isAndroid()` resolved, nothing happened — no spinner, no error, no feedback.
- **Fix**: Added `isAndroidPlatform === null` to the button's `disabled` condition so it is visually inactive until the platform is known.

#### Desktop serial framing drops `0x28` (CMD_BLE_TOGGLE) response as unknown byte
- **Root cause**: The `KNOWN_CMDS` array in `serial.rs` (used to re-sync framing when a non-packet byte is seen) did not include `0x28`. When the board responds to a BLE_TOGGLE command on desktop, the framing loop discarded the leading `0x28` byte and attempted to re-parse the remaining bytes as a new packet, causing a framing desync.
- **Fix**: Added `0x28` to `KNOWN_CMDS` in `crates/radiodoge-core/src/serial.rs`.

#### `mobile_build_lora_settings_packet` — unchecked `power_dbm as u8` cast
- **Root cause**: The desktop `update_lora_settings` command clamps TX power with `.max(2).min(22) as u8`. The mobile equivalent did a raw `settings.power_dbm as u8` — an `i8` to `u8` cast without bounds validation. Negative values (e.g. -1) would wrap to large `u8` values (255), sending an invalid TX power byte to the board.
- **Fix**: Applied `.max(2).min(22) as u8` to match the desktop path.

#### SendTab success-clear timer overwrites form while user is typing a follow-up send
- **Root cause**: After a successful send, a 5-second `setTimeout` cleared `toAddress`, `amountDoge`, and `memo`. If the user started typing a second transaction before the 5-second window elapsed, the timer fired and wiped their in-progress input.
- **Fix**: The timer ID is now stored in `_successClearTimer`. At the start of each `sendTransaction()` call, the pending timer is cancelled so the clear never races the next send. The timer is also cancelled on component unmount (tab switch).

#### `HistoryTab.copyAddr` missing try/catch — unhandled clipboard exception
- **Root cause**: `navigator.clipboard.writeText()` was not wrapped in a try/catch. In sandboxed or permission-denied environments this throws a `NotAllowedError` which propagated as an uncaught rejection, leaving the copy icon in its default state with no feedback.
- **Fix**: Wrapped in try/catch matching the identical pattern used by `ReceiveTab.copyPacket`.

#### `mobile_push_bytes` accumulator unbounded — memory exhaustion on misbehaving board
- **Root cause**: `acc.extend_from_slice(&bytes)` had no size guard. In normal operation the packet-drain loop keeps the accumulator to a few hundred bytes. But a board stuck in a debug-print loop, or one that sends valid command-byte headers that never complete into packets, could grow the accumulator without bound — an OOM risk on Android.
- **Fix**: Added an 8 KB ceiling in `mobile_push_bytes`. If the new data would exceed it, the accumulator is cleared (framing recovers on the next valid packet) and a warning is printed to stderr.

#### `load_history_from_disk` silently discards tx history on JSON corruption
- **Root cause**: `serde_json::from_str(&json).unwrap_or_default()` returned an empty Vec if `tx_history.json` was corrupted (e.g., power loss during write). No log message was emitted, making the data loss invisible.
- **Fix**: Changed to a `match` block that prints an `eprintln!` warning with the parse error before falling back to an empty Vec.

#### `load_address_book` silently discards address book on JSON corruption
- **Root cause**: Identical pattern to the history bug — `unwrap_or_default()` on `address_book.json` parse failure.
- **Fix**: Same match-with-warning approach.

#### `wallet.rs` `build_transaction_payload` — no NaN/Infinity guard before `f64 → u64` cast
- **Root cause**: `(amount_doge * 1e8).round() as u64` had no pre-flight check. While `f64::NAN as u64` is now defined (= 0) and `f64::INFINITY as u64` saturates in Rust ≥1.45, both produce wrong packet data silently. The frontend and Tauri IPC filter these inputs in practice, but the backend should not trust the caller.
- **Fix**: Added `if !amount_doge.is_finite() || amount_doge < 0.0 { anyhow::bail!(...) }` before the cast.

### Added

#### BLE advertising toggle
- **`CMD_BLE_TOGGLE` (0x28)** — new protocol command added to `radio.rs`. Sends a 1-byte payload (`0x01` = enable, `0x00` = disable) to instruct the board to start or stop BLE advertising.
- **`build_ble_toggle(src, enable)`** — packet builder in `radio.rs`.
- **`set_ble_enabled(enable: bool)`** — new desktop Tauri command (writes via Rust serial).
- **`mobile_build_ble_toggle(enable: bool)`** — new mobile Tauri command (returns bytes for JS to write).
- **BLE Advertising card** added to `SettingsTab.svelte` — mirrors the WiFi toggle card; shows current state and lets the user enable/disable BLE advertising. Works on both Android (mobile path) and desktop.
- **BLE Advertising card** also added to `ConnectionPanel.svelte` connected view — visible immediately when connected via USB-C or BLE, without navigating to Settings.

#### `mobileSendBytes` bridge function
- `connection-bridge.ts` now exports `mobileSendBytes(data: Uint8Array)` — routes to `_bleWrite` or `_usbWrite` based on active transport. Used by all Android Settings actions.

#### BLE UUID normalization
- All BLE service and characteristic UUIDs in `connection-bridge.ts` normalized to lowercase (`6e400001-...`) — required by Android's BLE stack, which rejects uppercase UUIDs on some devices/API levels.

### Changed
- `serial.rs`: Added `update_board_settings()` and `update_board_mac()` cache-update helpers used by `mobile_push_bytes`.
- `mobile_push_bytes` `KNOWN_CMDS` list updated to include `0x28` (CMD_BLE_TOGGLE).

---

## [0.3.15] — 2026-04-13 — 📡 Android Status Bar + Packet Stats Fix — Much Safe Area. Very Data. Wow.

> **Nav bar no longer hides behind the Android system status bar. Dashboard packet counters and NavBar signal bars now update in real time on mobile USB/BLE.**
> Such env(safe-area-inset-top). Very stats. Much data. Wow. 🐕📡

### Fixed

#### Android system status bar overlap (critical — tabs were untappable in portrait)
- **`NavBar.svelte` was rendered behind the Android system status bar** — the nav `<nav>` element had `height: 56px; padding: 0 16px` with no awareness of the system status bar (typically 24–28dp on Android). Tauri Android runs with an edge-to-edge WebView, so the nav rendered behind the clock/battery/WiFi row and the top 24–28dp of tab icons were blocked by system UI.
- **Fix**: `height` changed to `calc(56px + env(safe-area-inset-top, 0px))` and `padding-top` changed to `env(safe-area-inset-top, 0px)`. CSS Flexbox respects the padding when computing item positions with `align-items: center`, so the tab row and logo are centered in the 56px *below* the safe area — the status bar region remains transparent system chrome. Zero visual change on desktop (safe-area-inset-top is 0px).

#### Mobile packet counters never updating (Dashboard / Signal Strength)
- **`mobile_push_bytes` in `lib.rs` never emitted `radio-stats-update`** — the desktop path emits this event every 2 seconds from a polling loop; there was no equivalent for the Android mobile path. As a result, `connection.stats` was always the initial default (`packets_received: 0`, `rssi: 0 = "Very Weak"`), and Dashboard/NavBar signal bars never reflected real activity.
- **Fix**: After each batch of packets is extracted in `mobile_push_bytes`, a running `mobile_packets_rx` counter in `AppState` is incremented and a `RadioStats` snapshot is emitted as `radio-stats-update`. The packet counter now reflects every received packet. RSSI remains 0 on USB (serial carries no signal-strength metadata) but will auto-update on any transport that passes a non-zero `rssi` to `mobile_push_bytes`.
- **Counters reset** to zero in `mobile_set_disconnected` so reconnecting starts from a clean slate.

#### Tab touch targets below Android minimum
- **`min-height`** on `:global(.tab-btn)` bumped from 44px to 48px to match Material Design touch-target guideline.
- **`touch-action: manipulation`** added to tab buttons to suppress the 300ms tap delay on Android WebView (the WebView default waits 300ms to detect double-tap-to-zoom before dispatching `click`; `manipulation` opts out and makes taps feel native).

### Changed
- `height: 100svh` on `.app-shell` supplemented with `height: 100dvh` (dynamic viewport height) — adjusts as the Android URL bar / navigation rail shows and hides, preventing the content area from being slightly taller than the visible window.
- Version bumped to `0.3.15` across all crates, `package.json`, `tauri.conf.json`, and app footer.

---

## [0.3.14] — 2026-04-13 — 📱 Mobile Layout Fix — Much Responsive. Very Clamp. Wow.

> **Android UI is now correctly sized and usable. Bottom status bar no longer overflows. NavBar port badge truncates. Tab touch targets meet Android guidelines. Hero scales with screen.**

### Fixed

#### Android layout (all were regressions from hardcoded desktop sizes)
- **Bottom status bar overflow** — was `height: 28px` with fixed `gap: 16px` and 7+ items; on a 360dp phone the total content width exceeded the viewport by 3–4×. Redesigned as a two-group flex layout: `.footer-left` takes all available space with `min-width: 0` so `.footer-port` can ellipsis-truncate long device paths (`/dev/bus/usb/001/002 · 1.1.1` → `🟢 /dev/bus/usb/...`); `.footer-right` holds the debug button and the "such decentralize" slogan, which is hidden via `@media (max-width: 520px)`.
- **NavBar port badge width** — `white-space: nowrap` with no `max-width` made the connected badge push tab buttons off-screen to the left. Added `max-width: clamp(72px, 22vw, 200px); overflow: hidden; text-overflow: ellipsis;` so the badge truncates at 22% of viewport width.
- **Tab touch targets** — tab buttons had only `8px 14px` padding in a 56px nav, giving ~40px tap height. Added `min-height: 44px` via `:global(.tab-btn)` to meet the Android 48dp touch-target guideline.
- **Hero image fixed size** — `width="180" height="180"` was a fixed 180px on all screens. Replaced with `width: clamp(80px, 28vw, 180px); height: auto;` so it scales to 28% of the viewport on phones (≈101px on a 360dp phone) without clipping.
- **Hero section margins** — `margin-bottom: 40px` and fixed inner padding reduced to `clamp()` variants so the connect card is visible without scrolling on short phones.

### Changed
- Root shell uses `height: 100svh` (small viewport height) with `100vh` fallback — semantically correct for full-screen Tauri Android WebView.
- `app-main` gets `-webkit-overflow-scrolling: touch` for momentum scrolling on Android.
- Footer `padding-bottom` uses `max(3px, env(safe-area-inset-bottom))` for home-indicator clearance on edge-to-edge devices.
- BLE empty-state message improved: "Board advertises as 'RadioDoge-X.X.X' (Nordic UART Service)" replaces the generic "make sure BLE is enabled" note.
- Stats display compacted to `↑N ↓N` (removed "sent"/"received" labels) to fit the narrow footer.
- Version bumped to `0.3.14` across all crates, `package.json`, `tauri.conf.json`, and app footer.

---

## [0.3.13] — 2026-04-13 — 📶 BLE UUID Alignment + Disconnect Fix — Much NUS. Very GATT. Wow.

> **BLE now connects and exchanges data correctly. UUIDs aligned to Nordic UART Service. Unexpected disconnects reset UI. "Experimental" labels removed.**
> Such Nordic. Very UUID. Much connect. Wow. 🐕📶

### Fixed

#### BLE GATT UUIDs (blocker — BLE was completely non-functional)
- **Placeholder UUIDs replaced with Nordic UART Service (NUS) UUIDs** in `connection-bridge.ts`:
  - `BLE_SERVICE_UUID    = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E'`
  - `BLE_WRITE_CHAR_UUID = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E'` (NUS RX — host writes)
  - `BLE_NOTIFY_CHAR_UUID = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E'` (NUS TX — board notifies)
- The firmware (`heltec-firmware-v3/heltec-firmware.ino`) already implements NUS correctly; the GUI now uses matching UUIDs. BLE scan will find boards immediately; characteristic reads/writes and notifications will work.

#### Unexpected BLE Disconnect
- **`setDisconnected()` now called in the blec `onDisconnect` callback** — if the board drops the BLE connection unexpectedly, `activeBleAddress` is cleared and the global `connection.isConnected` becomes `false` immediately. Previously the UI stayed stuck showing "Connected" with no way to reconnect.

### Changed

- **Firmware BLE comment updated** in `heltec-firmware-v3/heltec-firmware.ino` to document v0.3.13 UUID alignment.
- **"USB-C is experimental"** warning removed from the empty-state device list message — USB-C OTG is stable.
- **Mobile layout**: outer padding now uses `clamp()` so the connect card uses full width on small phone screens without clipping on tablets.
- Version bumped to `0.3.13` across all crates, `package.json`, `tauri.conf.json`, and app footer.

---

## [0.3.12] — 2026-04-13 — 🔌 Global State Sync Fix — Much Connect. Very isConnected. Wow.

> **Critical bug fix: `connection.isConnected` now becomes `true` immediately after USB or BLE connects, so the Disconnect button appears and all isConnected-gated UI unlocks.**
> Such sync. Very state. Much connect. Wow. 🐕🔌

### Fixed

#### Global State Sync (critical)
- **`connection.isConnected` never becoming `true` on Android** — `setMobileConnected()` only sets `mobileStatus = 'connected'`; it never set `isConnected = true`. The Disconnect button, isConnected-gated tabs, and status bar all stayed stuck in the "disconnected" state regardless of whether the port was actually open.
- **Fix**: `connection-bridge.ts` now imports `setConnected` and `setDisconnected` from `$lib/stores/connection.svelte` and calls them directly:
  - `setConnected(devicePath, null)` immediately after USB-OTG port opens and listeners are registered
  - `setConnected(address, null)` immediately after BLE GATT connects and listeners are registered
  - `setDisconnected()` in the `notifyRust=true` paths of `_disconnectAndroid` and `_disconnectBluetoothAndroid`
- Board handshake responses (`connection-status: connected` event) will still fire asynchronously and re-call `setConnected` with the real `nodeAddress` and `firmwareVersion` when the board replies to `GET_SETTINGS`.

#### USB-C Disconnect Button
- Now shows correctly once the port opens (was broken by the above `isConnected` bug — no separate fix needed).

### Changed

- **BT tab subtitle**: `'Experimental'` → `'BLE'` — Bluetooth is fully implemented; no longer experimental.
- **BLE info box**: Removed the "Note: firmware GATT UUIDs are placeholders" developer note from the user-facing connect panel.
- Version bumped to `0.3.12` across all crates, `package.json`, `tauri.conf.json`, and app footer.

---

## [0.3.11] — 2026-04-13 — 📶 Full Android BLE + USB-C Write Fix — Much Wireless. Very GATT. Wow.

> **Bluetooth BLE is now fully implemented. USB-C serial write is fixed. Both paths share the same zero-duplication packet pipeline.**
> Such BLE. Very GATT. Much wire. Wow. 🐕📶

### Fixed

#### USB-C Serial Write (blocker)
- **`activePort.write(Uint8Array)` → `activePort.writeBinary(Uint8Array)`** — The `tauri-plugin-serialplugin` `write` command expects a `String` on the Rust side; passing a `Uint8Array` serialised as a JSON sequence caused "invalid args `value` for command `write`: invalid type: sequence, expected a string" at runtime. Fixed all four call-sites in `connection-bridge.ts` to use `writeBinary()` which routes to the `write_binary` Rust command that correctly accepts `Vec<u8>`. USB-C serial is now fully operational.

### Added

#### Android Bluetooth BLE (`tauri-plugin-blec` v0.4)
- **`bleScan(timeoutMs)`** — calls `mobile_ble_scan` (Rust state update) then `blec.scan(5000)`, returns discovered devices as `MobileDeviceInfo[]`
- **`_connectBluetoothAndroid(address)`** — `blec.connect()` → `blec.onReceiveData(SERVICE, NOTIFY_CHAR, cb)` where the notification callback calls `mobile_push_bytes` — **BLE notifications feed the identical Rust accumulator as USB bytes**
- **`_bleWrite(data)`** — logs to debug traffic via `mobile_ble_write_characteristic`, then calls `blec.sendData(SERVICE, WRITE_CHAR, data)`
- `mobilePing`, `mobileSendTransaction`, `mobileSendLoraSettings` all auto-route through `_bleWrite` or USB based on `activeBleAddress`
- Lazy `import('@mnlphlp/plugin-blec')` — BLE module never initialises on desktop startup
- **Rust BLE commands** registered in `lib.rs` and app builder: `mobile_ble_scan`, `mobile_ble_connect`, `mobile_ble_disconnect`, `mobile_ble_write_characteristic`
- `AppState.ble_device_address` tracks active BLE MAC address
- `blec:default` added to both desktop and mobile capabilities
- Scan button shows "Scanning BLE… (5 s)" during discovery; BLE tab info box updated from stub notice to functional description
- **Firmware GATT UUIDs** are clearly marked placeholder constants at the top of `connection-bridge.ts` — one-line swap when firmware finalises them

### Changed

- Version bumped to `0.3.11` across all crates, `package.json`, `tauri.conf.json`, and app footer.
- README Bluetooth section rewritten to document the implemented architecture.
- `patch_manifest.py` comment updated (BLE permissions are now required, not experimental).

---

## [0.3.10] — 2026-04-13 — 🔌 ESP32 USB-C Serial + Android USB-OTG — Much Wire. Very Serial. Wow.

> **RadioDoge now talks to ESP32/Heltec boards via USB-C on both desktop and Android.**
> Such serial. Very OTG. Much mesh. Wow. 🐕🔌

### Added

#### Android USB-C Serial (USB-OTG)
- **`tauri-plugin-serialplugin = "=2.22.0"`** — Android USB-OTG serial bridge via `usb-serial-for-android` (JitPack); pinned to =2.22.0 because v2.20.0 reads non-existent `baudRate`/`dataBits`/`parity` getter properties on `UsbSerialPort` causing fatal Kotlin compile errors
- **JitPack CI patch** — `.github/scripts/patch_jitpack.py` injects `maven { url = uri("https://jitpack.io") }` after every `mavenCentral()` in the Tauri-generated `build.gradle.kts`; required because the generated file only lists `google()` + `mavenCentral()`
- **`tauri-plugin-os = "2"`** — runtime platform detection (`"android"`, `"windows"`, `"linux"`, …) used for USB-OTG vs. desktop serial branching

#### Bluetooth BLE Framework Stub
- **Bluetooth BLE permission patch** — `.github/scripts/patch_manifest.py` adds `BLUETOOTH`, `BLUETOOTH_CONNECT`, `BLUETOOTH_SCAN`, and `ACCESS_FINE_LOCATION` permissions to the generated `AndroidManifest.xml` ahead of future BLE scanning UI
- BLE scanning infrastructure stubbed; no active BLE connections in this release

#### Mobile Bridge Architecture
- **`mobile_push_bytes`** — Tauri command that accepts raw byte arrays from the frontend and forwards them to the Android serial plugin
- **`mobile_build_ping`** — returns a pre-built ping packet as `Result<Vec<u8>, String>` for the mobile frontend to send via `serialplugin`
- **`mobile_build_connect_queries`** — returns firmware-version + settings query packets as `Result<Vec<Vec<u8>>, String>`
- All three async commands return `Result<_, String>` as required by Tauri v2's `AsyncCommandMustReturnResult` trait

#### Platform-Abstracted Serial (`connection-bridge.ts`)
- `ConnectionBridge` TypeScript abstraction routes serial I/O through `SerialManager` on desktop and `tauri-plugin-serialplugin` on Android
- Mobile status badge displayed in `ConnectionPanel` when running on Android

#### Updated CI (`build-android.yml`)
- Complex CI logic extracted to `.github/scripts/` Python files to avoid YAML literal block scalar termination bugs (Python heredoc bodies at column 0 break `run: |`)
- `@tauri-apps/cli: "^2"` (was `"^2.3.0"`) — CLI version must match Rust `tauri` crate major.minor; mismatch caused Kotlin `onDetach overrides nothing` error
- Retry loop (3 attempts, 15 s / 30 s backoff) on APK build step
- Build log always uploaded as artifact

### Fixed

- **`mobile_build_ping` / `mobile_build_connect_queries` E0277** — async `#[tauri::command]` functions taking `State<'_, AppState>` must return `Result<T, E>`; fixed return types from bare `Vec<>` to `Result<Vec<>, String>`
- **`capabilities/mobile.json` `"ios"` casing** — Tauri requires `"iOS"` (capital S); lowercase caused `unknown variant 'ios'` JSON parse failure breaking both MSI and APK builds
- **Cargo.toml `log`/`env_logger` section placement** — kept in main `[dependencies]` table, not inside `[target.'cfg(not(target_os = "android"))'.dependencies]`

### Changed

- Version bumped to `0.3.10` across all crates, `package.json`, and app footer.
- `tauri = "^2.6.2"` floor pin ensures Kotlin runtime is always ≥ 2.6.2 (serialplugin requirement).

---

## [0.3.9] — 2026-03-15 — 📱 Android Port + Mobile UI — Much Mobile. Very LoRa. Wow.

> **RadioDoge now runs on Android. Install the debug APK directly from CI artifacts.**
> Such cross-platform. Very Tauri Mobile. Much mesh. Wow. 🐕📱

### Added

#### Android App (Tauri Mobile)
- **`build-android.yml`** — GitHub Actions workflow builds a debug APK on every push via `tauri android build --apk --debug`
- Debug APK is auto-signed by Gradle (installable on any Android 7.0+ / API 24+ device without a Play Store release)
- APK artifact uploaded as `radiodoge-android-debug-apk-vX.X.X`, retained 30 days
- Build log always uploaded as artifact for debugging

#### Mobile-Responsive UI
- **NavBar icon-only mode** — on screens ≤640px wide, tab labels are hidden; all 9 tabs show as icons only, fitting within phone width without clipping
- **NavBar horizontal scroll** — `overflow-x: auto` + hidden scrollbar as a safety net if icons still overflow
- **`flex-shrink: 0`** on all tab buttons — buttons never compress on narrow screens
- **Logo text hidden** on mobile — keeps the bouncing Doge icon, drops the "RadioDoge / WIRELESS P2P" text block
- **`viewport-fit=cover`** — correct handling of notches and rounded screen corners on modern Android
- **`user-scalable=no`** — prevents accidental pinch-zoom (appropriate for a native-app WebView)

### Fixed

#### Cargo.toml: `log` and `env_logger` under wrong dependency section
- **Bug**: A previous edit inserted `[target.'cfg(not(target_os = "android"))'.dependencies]` in the middle of `Cargo.toml`. Because TOML is order-sensitive, `log = "0.4"` and `env_logger = "0.11"` — which appeared after that section header — were silently moved into the target-specific section. On Android builds they were invisible to the compiler, causing 15 `E0433: use of unresolved module or unlinked crate` errors.
- **Fix**: Moved `log` and `env_logger` back above the target-specific section header, into the main `[dependencies]` table.

### Changed

- Version bumped to `0.3.9` across all crates, `tauri.conf.json`, `package.json`, and app footer.

---

## [0.3.8] — 2026-03-14 — 💾 Persistent Wallet + Battery + Theme — Much Persist. Very Battery. Wow.

> **Wallet survives app restarts. Battery level on screen. Light theme for the purists.**
> Such persistence. Very power. Much theme. Wow. 🐕

### Added

#### Persistent Wallet
- **Save wallet to disk** — encrypted wallet file stored in Tauri app data directory
- **Load wallet on launch** — if a saved wallet exists, it is loaded automatically on startup
- **Scary confirmation modal** — `<!-- v0.3.8 — Scary save confirmation modal -->` before overwriting with big red warning that private key security is the user's responsibility
- Hot wallet warning logged at `WARN` level on every save

#### Battery Voltage Display
- **Dashboard battery card** — polls `CMD_GET_BATTERY` every 15 seconds when connected
- Shows voltage in mV with a human-readable percentage estimate
- Graceful fallback: "Connect to a v0.3.8+ board to see this" if the board doesn't respond

#### Board MAC Address
- **Settings tab** — displays the board's MAC address string (e.g. `A0:B1:C2:D3:E4:F5`)
- Queried on connect via `CMD_GET_MAC_ADDR`; copy-to-clipboard button included

#### Light/Dark Theme Toggle
- **Settings tab** — full light theme override via `[data-theme="light"]` CSS variables
- Persisted to `localStorage`; applied on next launch
- All brand colors (backgrounds, borders, text, glow effects) have light equivalents

#### Address Book Persistence
- `address_book.json` stored in Tauri app data directory
- Entries survive app restarts

### Changed

- Version bumped to `0.3.8` across all crates, `tauri.conf.json`, `package.json`, and app footer.

---

## [0.3.7] — 2026-03-13 — 🕸️ Mesh Neighbors + WiFi Toggle + Address Conflict — Much Mesh. Very Radar. Wow.

> **See who else is on the mesh. Toggle WiFi on the board. Know when there's an address clash.**
> Such neighbors. Very topology. Much conflict. Wow. 🐕

### Added

#### Mesh Neighbor Tracking
- **Mesh tab** — live list of recently heard nodes on the LoRa network
- Each entry shows node address, last-heard timestamp, RSSI, and SNR
- Auto-refreshed from the serial read loop whenever a valid packet is received from a new source

#### Address Conflict Detection
- **CMD 0x25** — firmware emits an address-conflict notification when it detects a duplicate node address on the mesh
- GUI listens for `addr-conflict` Tauri event; sets `addrConflict` flag in the connection store
- **Auto-switches to Mesh tab** when a conflict is detected so the user sees the warning immediately

#### WiFi Radio Toggle
- **Settings tab** — enable/disable the board's WiFi radio via a toggle
- Sends `CMD_SET_WIFI_ENABLED` to the board; board persists the setting in NVS
- Reflected in the connection store as `wifiEnabled`; toggle shows current board state

#### Address Book UI
- **Addresses tab** — save and label Dogecoin addresses for quick access in the Send tab
- Each entry: label, Dogecoin address, optional notes, copy button
- Stored in memory (persisted to disk in v0.3.8)

### Changed

- Version bumped to `0.3.7` across all crates, `tauri.conf.json`, `package.json`, and app footer.

---

## [0.3.6] — 2026-03-12 — 🌐 Gateway Mode + Board Sync — Much Gateway. Very Sync. Wow.

> **Board is now the source of truth. Gateway daemon runs from the GUI. Much robust.**
> Such sync. Very gateway. Much daemon. Wow. 🐕

### Added

#### Board Sync
- **`CMD_GET_SETTINGS` (0x22)** — queried automatically on every successful connect
- Board replies with current node address and gateway mode flag — board is source of truth, not the GUI's local state
- `board-sync` Tauri event updates the connection store atomically; NavBar and footer refresh immediately
- Eliminates the previous bug where the GUI could show a stale address after a reconnect

#### Gateway Mode
- **Settings tab** — enable/disable gateway mode on the board via toggle
- Board stores the setting in NVS; GUI reflects it via board-sync on next connect
- NavBar shows a `🌐 GATEWAY` badge when the board is in gateway mode
- `setGatewayMode` + `setConfirmedGatewayMode` store functions track pending vs. confirmed state

#### Gateway Daemon
- **Settings tab** — "▶ Start Daemon" button spawns `radiodoge-cli daemon` as a child process
- NavBar shows a `▶ Daemon` badge when the daemon is running
- `gateway-status` Tauri event (`{ online: boolean }`) tracks daemon lifecycle
- Daemon can be stopped from the GUI; process is cleaned up on app exit

### Changed

- Version bumped to `0.3.6` across all crates, `tauri.conf.json`, `package.json`, and app footer.

---

## [0.3.5] — 2026-03-08 — ✨ Export Toast + Send Tab Validation — Much Safety. Very Feedback. Wow.

> **Two targeted additions on top of v0.3.4 — zero behavior regressions.
> All changes additive and backward-compatible.** 🐕

### Added

#### Debug Console Export Toast — You Know Exactly Where the File Went
- **Previous behavior**: `exportToTxt()` triggered the browser download silently.
  The user had no confirmation that anything happened or what the file was named.
- **Now**: A green `💾 Saved to Downloads/radiodoge-debug-2026-03-08T06-50-05.txt`
  toast slides up above the debug console immediately after the download fires,
  auto-dismisses after 4 seconds, uses the same `slide-up` animation, and is
  announced to screen-readers via `role="status" aria-live="polite"`.

#### Send Tab — Hard "From Wallet" Gate
- **Previous behavior**: `canSend()` did NOT require `wallet.isGenerated`.
  Pressing Send with no wallet loaded would pass `fromPrivateKeyWif: undefined`
  to the backend — a broken or unsigned packet.
- **Now**: `canSend()` includes `wallet.isGenerated`. `sendTransaction()` also
  has an explicit early-return guard with a clear inline error message before
  the `invoke()` — belt-and-suspenders.
- **From wallet row**: orange "optional for MVP" hint → red actionable block
  that links users to the Wallet tab. Turns green ✅ when a wallet is loaded.
- **Send button**: context-sensitive `title` tooltip explains exactly why it is
  disabled at each stage (no device → no wallet → bad address → bad amount → ready).

### Changed

- Version bumped to `0.3.5` in `package.json`, all `Cargo.toml` files,
  `tauri.conf.json`, and app footer (`+page.svelte`).

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
