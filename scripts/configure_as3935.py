#!/usr/bin/env python3
"""
Configure AS3935 Lightning Detector via REST API

Usage:
    python configure_as3935.py --url http://192.168.4.1 [--spi-host 1] [--sclk 14] [--mosi 13] [--miso 12] [--cs 15] [--irq 10]
"""

import argparse
import json
import requests
import sys

def configure_as3935_pins(base_url, spi_host=1, sclk=14, mosi=13, miso=12, cs=15, irq=10):
    """Configure AS3935 pins via REST API"""
    
    endpoint = f"{base_url}/api/as3935/pins/save"
    payload = {
        "spi_host": spi_host,
        "sclk": sclk,
        "mosi": mosi,
        "miso": miso,
        "cs": cs,
        "irq": irq
    }
    
    print(f"Configuring AS3935 pins...")
    print(f"  SPI Host: {spi_host}")
    print(f"  SCLK Pin: {sclk}")
    print(f"  MOSI Pin: {mosi}")
    print(f"  MISO Pin: {miso}")
    print(f"  CS Pin: {cs}")
    print(f"  IRQ Pin: {irq}")
    print(f"\nSending POST to {endpoint}")
    
    try:
        response = requests.post(endpoint, json=payload, timeout=5)
        if response.status_code == 200:
            print(f"✓ Successfully configured AS3935 pins")
            print(f"Response: {response.text}")
            return True
        else:
            print(f"✗ Failed to configure pins (HTTP {response.status_code})")
            print(f"Response: {response.text}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"✗ Connection error: {e}")
        return False

def get_as3935_status(base_url):
    """Get AS3935 configuration status"""
    
    endpoint = f"{base_url}/api/as3935/status"
    print(f"\nFetching AS3935 status from {endpoint}")
    
    try:
        response = requests.get(endpoint, timeout=5)
        if response.status_code == 200:
            data = response.json()
            print(f"✓ Current AS3935 status:")
            print(json.dumps(data, indent=2))
            return True
        else:
            print(f"✗ Failed to get status (HTTP {response.status_code})")
            print(f"Response: {response.text}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"✗ Connection error: {e}")
        return False

def get_as3935_pins_status(base_url):
    """Get AS3935 pins configuration status"""
    
    endpoint = f"{base_url}/api/as3935/pins/status"
    print(f"\nFetching AS3935 pins status from {endpoint}")
    
    try:
        response = requests.get(endpoint, timeout=5)
        if response.status_code == 200:
            data = response.json()
            print(f"✓ Current AS3935 pins:")
            print(json.dumps(data, indent=2))
            return True
        else:
            print(f"✗ Failed to get pins status (HTTP {response.status_code})")
            print(f"Response: {response.text}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"✗ Connection error: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description="Configure AS3935 Lightning Detector via REST API")
    parser.add_argument("--url", required=True, help="Base URL of the device (e.g., http://192.168.4.1)")
    parser.add_argument("--spi-host", type=int, default=1, help="SPI host number (default: 1)")
    parser.add_argument("--sclk", type=int, default=14, help="SCLK pin number (default: 14)")
    parser.add_argument("--mosi", type=int, default=13, help="MOSI pin number (default: 13)")
    parser.add_argument("--miso", type=int, default=12, help="MISO pin number (default: 12)")
    parser.add_argument("--cs", type=int, default=15, help="CS pin number (default: 15)")
    parser.add_argument("--irq", type=int, default=10, help="IRQ pin number (default: 10)")
    parser.add_argument("--status-only", action="store_true", help="Only fetch current status, don't configure")
    
    args = parser.parse_args()
    
    # Normalize URL
    base_url = args.url.rstrip('/')
    
    if args.status_only:
        success = get_as3935_pins_status(base_url)
        get_as3935_status(base_url)
        return 0 if success else 1
    
    # Configure pins
    success = configure_as3935_pins(
        base_url,
        spi_host=args.spi_host,
        sclk=args.sclk,
        mosi=args.mosi,
        miso=args.miso,
        cs=args.cs,
        irq=args.irq
    )
    
    if success:
        # Get updated status
        get_as3935_pins_status(base_url)
        get_as3935_status(base_url)
        return 0
    else:
        return 1

if __name__ == "__main__":
    sys.exit(main())
