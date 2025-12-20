# Building libdogecoin WASM for RadioDoge

This guide explains how to build a WebAssembly (WASM) version of libdogecoin for use in the RadioDoge ESP32 web interface.

## Prerequisites

1. **Emscripten SDK** - Required for compiling C to WASM
   - Download from: https://emscripten.org/docs/getting_started/downloads.html
   - Follow installation instructions for your OS

2. **libdogecoin Source** - Clone the repository
   ```bash
   git clone https://github.com/dogecoinfoundation/libdogecoin.git
   cd libdogecoin
   ```

3. **Build Dependencies** - Install required libraries
   - **Debian/Ubuntu**: `sudo apt-get install autoconf automake libtool build-essential libevent-dev`
   - **macOS**: `xcode-select --install` and install Homebrew dependencies

## Building libdogecoin Library

### Step 1: Build libdogecoin Static Library

First, build the native libdogecoin library:

```bash
cd libdogecoin
./autogen.sh
./configure --disable-net --disable-tools
make
```

This creates `libdogecoin.a` in the `/.libs` directory.

### Step 2: Build Dependencies

You'll also need:
- **libsecp256k1**: Already included as a git subtree in `../src/secp256k1/`
- **libunistring**: Build separately or use pre-built version

#### Building libunistring (if needed):

```bash
# Download and build libunistring
cd /tmp
wget https://ftp.gnu.org/gnu/libunistring/libunistring-1.1.tar.xz
tar -xf libunistring-1.1.tar.xz
cd libunistring-1.1
./configure --prefix=/tmp/libunistring-1.1/build
make
make install
```

## Building WASM with Emscripten

### Step 3: Set Up Emscripten Environment

```bash
# Activate Emscripten (adjust path to your installation)
source ~/emsdk/emsdk_env.sh  # Linux/Mac
# or
emsdk_env.bat  # Windows
```

### Step 4: Compile to WASM

Use the following Emscripten command to compile libdogecoin to WASM:

```bash
emcc -s STRICT=1 \
    -s EXPORTED_FUNCTIONS='["_dogecoin_ecc_start","_dogecoin_ecc_stop","_moon","_sign_message","_verify_message","_generatePrivPubKeypair","_generateHDMasterPubKeypair","_generateDerivedHDPubkey","_getDerivedHDAddressByPath","_getDerivedHDAddress","_getDerivedHDAddressFromMnemonic","_verifyPrivPubKeypair","_verifyHDMasterPubKeypair","_verifyP2pkhAddress","_generateHDMasterPubKeypairFromMnemonic","_verifyHDMasterPubKeypairFromMnemonic","_start_transaction","_add_utxo","_add_output","_finalize_transaction","_get_raw_transaction","_clear_transaction","_remove_all","_sign_raw_transaction","_sign_transaction","_sign_transaction_w_privkey","_store_raw_transaction","_generateEnglishMnemonic","_generateRandomEnglishMnemonic","_dogecoin_seed_from_mnemonic","_qrgen_p2pkh_to_qrbits","_qrgen_p2pkh_to_qr_string","_qrgen_p2pkh_consoleprint_to_qr","_qrgen_string_to_qr_pngfile","_qrgen_string_to_qr_jpgfile","_koinu_to_coins_str","_coins_to_koinu_str","_dogecoin_get_balance","_dogecoin_get_balance_str","_dogecoin_get_utxo_txid_str","_dogecoin_get_utxos_length","_chain_from_b58_prefix_bool","_dogecoin_char_vla","_dogecoin_free","_malloc","_free"]' \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","setValue","getValue","UTF8ToString","stackAlloc","intArrayFromString","stringToUTF8"]' \
    -s MODULARIZE=1 \
    -s ENVIRONMENT='web' \
    -s EXPORT_NAME="loadWASM" \
    -s SINGLE_FILE=1 \
    libdogecoin.a \
    ../src/secp256k1/.libs/libsecp256k1.a \
    /tmp/libunistring-1.1/build/lib/libunistring.a \
    -I../include/dogecoin \
    -o ../libdogecoin.js
```

### Explanation of Emscripten Flags

- `-s STRICT=1`: Enable strict mode for better error checking
- `-s EXPORTED_FUNCTIONS`: List of C functions to export (prefixed with `_`)
- `-s EXPORTED_RUNTIME_METHODS`: JavaScript helper functions to include
- `-s MODULARIZE=1`: Create a module that can be imported
- `-s ENVIRONMENT='web'`: Target web environment
- `-s EXPORT_NAME="loadWASM"`: Name of the exported module function
- `-s SINGLE_FILE=1`: Embed WASM binary in the JS file (single file output)
- `-I../include/dogecoin`: Include path for libdogecoin headers

### Step 5: Verify Output

After compilation, you should have:
- `libdogecoin.js`: The WASM module (large file, ~2-3MB)

## Alternative: Lite Version

For a smaller file size, you can build a "lite" version with fewer functions:

```bash
emcc \
  -s EXPORTED_FUNCTIONS='["_dogecoin_ecc_start","_dogecoin_ecc_stop","_moon","_sign_message","_verify_message","_generatePrivPubKeypair","_verifyPrivPubKeypair","_verifyP2pkhAddress","_start_transaction","_add_utxo","_add_output","_finalize_transaction","_get_raw_transaction","_clear_transaction","_remove_all","_sign_raw_transaction","_sign_transaction","_sign_transaction_w_privkey","_koinu_to_coins_str","_coins_to_koinu_str","_chain_from_b58_prefix_bool","_dogecoin_char_vla","_dogecoin_free","_malloc","_free"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","setValue","getValue","UTF8ToString","stackAlloc","intArrayFromString","stringToUTF8"]' \
  -s MODULARIZE=1 \
  -s ENVIRONMENT=web \
  -s EXPORT_NAME=loadWASM \
  -s SINGLE_FILE=1 \
  libdogecoin.a \
  ../src/secp256k1/.libs/libsecp256k1.a \
  /tmp/libunistring-1.1/build/lib/libunistring.a \
  -I../include/dogecoin \
  -o ../libdogecoin-lite.js
```

## Uploading to RadioDoge

### Step 6: Copy to Data Folder

Copy the generated `libdogecoin.js` file to your RadioDoge firmware `data` folder:

```bash
cp libdogecoin.js /path/to/radiodoge/heltec-firmware/data/
```

### Step 7: Upload Filesystem

1. Open Arduino IDE
2. Select your RadioDoge board
3. Go to `Tools > ESP32 Sketch Data Upload`
4. Wait for upload to complete

**Note**: Make sure you have a partition table with enough LittleFS space (see `README_PARTITIONS.md`)

## Using in Web Interface

The WASM file will be available at `/libdogecoin.js` in the web interface. Load it using:

```javascript
const Module = await loadWASM();
// Now you can use Module.ccall() to call libdogecoin functions
```

## Troubleshooting

### "File system is full" error
- Increase LittleFS partition size (see `README_PARTITIONS.md`)
- Or use the lite version

### WASM fails to load
- Check browser console for errors
- Verify file was uploaded correctly
- Ensure Emscripten version is compatible

### Functions not available
- Check that functions are in `EXPORTED_FUNCTIONS` list
- Rebuild with correct function names (prefixed with `_`)

### Build errors
- Ensure all dependencies are built
- Check that libsecp256k1 and libunistring paths are correct
- Verify Emscripten is properly activated

## References

- **libdogecoin**: https://github.com/dogecoinfoundation/libdogecoin
- **Emscripten**: https://emscripten.org/
- **Getting Started Guide**: See `doc/getting_started.md` in libdogecoin repo




