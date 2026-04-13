#!/usr/bin/env python3
"""Inject JitPack into the Tauri-generated Android build.gradle.kts.

tauri-plugin-serialplugin depends on usb-serial-for-android which is hosted
on JitPack.  The generated build.gradle.kts only includes google() and
mavenCentral(), so we add the JitPack maven block after every mavenCentral()
call — covering both the buildscript{} and allprojects{} blocks.

Called from .github/workflows/build-android.yml after `tauri android init`.
Paths are relative to the repository root.
"""
import re
import sys
from pathlib import Path

path = Path("radiodoge-gui/src-tauri/gen/android/build.gradle.kts")

if not path.exists():
    print(f"WARNING: {path} not found — skipping JitPack patch", file=sys.stderr)
    sys.exit(0)

content = path.read_text()

if "jitpack.io" in content:
    print("JitPack already present — no patch needed")
    sys.exit(0)

jitpack_line = '        maven { url = uri("https://jitpack.io") }'

patched = re.sub(
    r'([ \t]+mavenCentral\(\))([ \t]*\n)([ \t]+\})',
    lambda m: f'{m.group(1)}{m.group(2)}{jitpack_line}\n{m.group(3)}',
    content,
)

if patched == content:
    print("WARNING: pattern not found — JitPack may need to be added manually",
          file=sys.stderr)
    sys.exit(0)

path.write_text(patched)
print("JitPack added to build.gradle.kts")
