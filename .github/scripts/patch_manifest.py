#!/usr/bin/env python3
"""Add Bluetooth permissions to the Tauri-generated AndroidManifest.xml.

USB permissions are added automatically by tauri-plugin-serialplugin.
These BLE permissions enable future Bluetooth scanning from the UI.

Called from .github/workflows/build-android.yml after `tauri android init`.
Paths are relative to the repository root.
"""
import sys
from pathlib import Path

path = Path(
    "radiodoge-gui/src-tauri/gen/android/app/src/main/AndroidManifest.xml"
)

if not path.exists():
    print(f"WARNING: {path} not found — skipping BT permission patch",
          file=sys.stderr)
    sys.exit(0)

content = path.read_text()

if "BLUETOOTH_CONNECT" in content:
    print("Bluetooth permissions already present — no patch needed")
    sys.exit(0)

bt_perms = (
    "    <!-- Bluetooth BLE scanning (experimental, v0.3.10) -->\n"
    '    <uses-permission android:name="android.permission.BLUETOOTH"'
    ' android:maxSdkVersion="30" />\n'
    '    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />\n'
    '    <uses-permission android:name="android.permission.BLUETOOTH_SCAN" />\n'
    '    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />\n'
)

patched = content.replace("<application", bt_perms + "\n    <application", 1)
path.write_text(patched)
print("Bluetooth permissions added to AndroidManifest.xml")
