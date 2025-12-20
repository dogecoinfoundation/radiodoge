#!/usr/bin/env python3
"""
Heltec ESP32 License Setter
Sends the license key to Heltec ESP32 device via AT command
"""

import serial
import time
import sys

# License key (concatenated hex values without 0x prefix)
LICENSE_KEY = "5642DDFECEB390B19B6F409CBA24DE60"

# Serial port configuration
SERIAL_PORT = "COM3"
BAUD_RATE = 115200
TIMEOUT = 5

def set_heltec_license(port=SERIAL_PORT, baud=BAUD_RATE, license_key=LICENSE_KEY):
    """
    Set Heltec license via AT command
    
    Args:
        port: Serial port (e.g., 'COM3' on Windows, '/dev/ttyUSB0' on Linux)
        baud: Baud rate (default 115200)
        license_key: License key as hex string without 0x prefix
    """
    try:
        print(f"Connecting to {port} at {baud} baud...")
        
        # Open serial port
        ser = serial.Serial(port, baud, timeout=TIMEOUT)
        time.sleep(2)  # Wait for connection to stabilize
        
        print("Connected!")
        print(f"Setting license key: {license_key}")
        print("-" * 50)
        
        # Clear any existing data in buffer
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        time.sleep(0.5)
        
        # Send AT command to set license
        command = f"AT+CDKEY={license_key}\r\n"
        print(f"Sending: {command.strip()}")
        ser.write(command.encode())
        ser.flush()
        
        # Wait for response
        time.sleep(1)
        
        # Read response
        response = ""
        start_time = time.time()
        while time.time() - start_time < TIMEOUT:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                response += data
                print(data, end='')
                if "OK" in response or "ERROR" in response:
                    break
            time.sleep(0.1)
        
        print("-" * 50)
        
        # Check response
        if "OK" in response:
            print("✓ License set successfully!")
            print("\nPlease reset your device (or power cycle) for the license to take effect.")
            return True
        elif "ERROR" in response:
            print("✗ Error setting license. Check the response above.")
            return False
        else:
            print("⚠ No clear response received. License may or may not be set.")
            print("Try resetting the device and check if the license error is gone.")
            return False
            
    except serial.SerialException as e:
        print(f"✗ Serial port error: {e}")
        print(f"\nTroubleshooting:")
        print(f"  - Make sure {port} is the correct port")
        print(f"  - Check if device is connected")
        print(f"  - Close other programs using {port} (Arduino IDE Serial Monitor, etc.)")
        return False
    except Exception as e:
        print(f"✗ Unexpected error: {e}")
        return False
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print(f"\nSerial port {port} closed.")

if __name__ == "__main__":
    # Allow command line arguments to override defaults
    port = sys.argv[1] if len(sys.argv) > 1 else SERIAL_PORT
    license_key = sys.argv[2] if len(sys.argv) > 2 else LICENSE_KEY
    
    print("=" * 50)
    print("Heltec ESP32 License Setter")
    print("=" * 50)
    print(f"Port: {port}")
    print(f"Baud Rate: {BAUD_RATE}")
    print(f"License Key: {license_key}")
    print("=" * 50)
    print()
    
    success = set_heltec_license(port, BAUD_RATE, license_key)
    
    sys.exit(0 if success else 1)

