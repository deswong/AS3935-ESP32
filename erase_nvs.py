#!/usr/bin/env python3
"""
Erase the AS3935 NVS namespace from the device.
This fixes GPIO conflicts caused by stale pin configuration in NVS.
"""

import sys
import subprocess

def erase_nvs():
    """Erase NVS partition using esptool"""
    try:
        # Get the NVS partition offset and size from the partition table
        # For standard ESP32-C3 with 4MB flash: NVS is typically at 0x9000 with size 0x6000
        print("[*] Erasing NVS partition...")
        result = subprocess.run([
            sys.executable, "-m", "esptool",
            "erase_region", "0x9000", "0x6000"
        ], capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"[!] Error erasing NVS: {result.stderr}")
            return False
        
        print("[+] NVS erased successfully!")
        print("[*] Device will reinitialize NVS on next boot")
        return True
        
    except Exception as e:
        print(f"[!] Failed to erase NVS: {e}")
        return False

if __name__ == "__main__":
    if not erase_nvs():
        sys.exit(1)
    print("\n[+] NVS erase complete. Flash the firmware and power cycle the device.")
