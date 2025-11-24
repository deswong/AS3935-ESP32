# AS3935 Configuration - Quick Reference

## Device Access Points

### In AP Mode (Factory/No Saved WiFi)
- **SSID:** AS3935-Setup
- **URL:** http://192.168.4.1
- **Mode:** Provisioning AP (no password)

### In STA Mode (After WiFi Configuration)
- **SSID:** Your configured WiFi network
- **URL:** Use device's assigned IP from your router (check router DHCP list or serial logs)
- **Example:** http://192.168.1.153

## Configuration Steps

### 1. Get Device IP
Check serial monitor for line like:
```
I (142391) esp_netif_handlers: sta ip: 192.168.1.153, mask: 255.255.255.0, gw: 192.168.1.1
```
Or use provisioning AP at `192.168.4.1`

### 2. Configure AS3935 Pins (Default Configuration)

#### PowerShell (Recommended)
```powershell
cd scripts
.\configure_as3935.ps1 -Url http://192.168.4.1
```

#### Quick curl command
```bash
curl -X POST http://192.168.4.1/api/as3935/pins/save \
  -H "Content-Type: application/json" \
  -d '{"spi_host":1,"sclk":14,"mosi":13,"miso":12,"cs":15,"irq":10}'
```

### 3. Verify Configuration
```bash
# PowerShell
.\configure_as3935.ps1 -Url http://192.168.4.1 -StatusOnly

# curl
curl http://192.168.4.1/api/as3935/pins/status
curl http://192.168.4.1/api/as3935/status
```

## Default Pins

| Signal | GPIO | Purpose |
|--------|------|---------|
| SCLK   | 14   | Serial clock (SPI) |
| MOSI   | 13   | Data to sensor (SPI) |
| MISO   | 12   | Data from sensor (SPI) |
| CS     | 15   | Chip select (SPI) |
| IRQ    | 10   | Interrupt signal |

## Custom Pin Configuration

If your hardware uses different pins:

```powershell
.\configure_as3935.ps1 -Url http://192.168.4.1 `
  -SpiHost 1 `
  -Sclk 21 `
  -Mosi 19 `
  -Miso 25 `
  -Cs 22 `
  -Irq 11
```

## Verify Everything Works

After configuration, check:

1. **Device connects to WiFi** - Serial log shows:
   ```
   I (142391) esp_netif_handlers: sta ip: 192.168.X.X
   ```

2. **AS3935 initialized** - Serial log shows no SPI errors

3. **Web UI accessible** - Open http://<device-ip>/ in browser

4. **API endpoints responding** - Try any API endpoint:
   ```bash
   curl http://<device-ip>/api/as3935/status
   ```

## Next: Configure MQTT

Once AS3935 is configured, configure MQTT publishing via:
- Web UI at http://<device-ip>/
- Or API: `POST /api/mqtt/save` with broker details

---

For detailed information, see:
- [AS3935_CONFIGURATION.md](AS3935_CONFIGURATION.md) - Full configuration guide
- [../README.md](../README.md) - Project overview
