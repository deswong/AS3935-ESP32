# Release Notes — v1.0.0

Production-ready release of AS3935 Lightning Monitor for ESP32.

## Release Summary

This is the first stable release of the AS3935 Lightning Monitor firmware. The system provides a complete lightning detection solution with web UI provisioning, MQTT event publishing, and comprehensive REST API for integration.

**Release Date:** November 2025  
**Status:** Stable, production-ready  
**ESP-IDF Support:** 6.x

## Features

- ✅ Lightning detection via AS3935 I2C sensor
- ✅ Responsive web UI for setup and configuration
- ✅ MQTT event publishing with real-time updates
- ✅ Complete REST API (20+ endpoints)
- ✅ Automatic NTP time synchronization
- ✅ Sensor calibration with parameter tuning
- ✅ I2C address and pin configuration
- ✅ Event streaming via Server-Sent Events (SSE)
- ✅ NVS persistence for all settings
- ✅ Captive portal for initial provisioning

## Quick Start

1. **Build and Flash**
   ```bash
   idf.py set-target esp32c3
   idf.py fullclean build
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

2. **Connect to Provisioning AP** at `192.168.4.1`
3. **Configure Wi-Fi and MQTT**
4. **Start monitoring** — device joins your network and publishes events

## Documentation

- **[GETTING_STARTED.md](GETTING_STARTED.md)** — Step-by-step setup guide
- **[README.md](README.md)** — Feature overview and configuration
- **[API_REFERENCE.md](API_REFERENCE.md)** — Complete REST API documentation

## Pre-Release Verification

Before deploying to production, verify:

- [ ] **Build verification**
  ```bash
  idf.py fullclean
  idf.py -DTEST=true build
  ```

- [ ] **Hardware validation**
  - Device boots and starts provisioning AP
  - Web UI loads and responds
  - Wi-Fi provisioning works
  - MQTT connection established
  - Lightning events detected and published

- [ ] **API validation**
  - All REST endpoints respond correctly
  - Calibration procedure completes
  - Settings persist across reboots

- [ ] **Documentation review**
  - Setup guides are clear and complete
  - API reference covers all endpoints
  - Troubleshooting addresses common issues

## Known Limitations

- **No Authentication**: REST API has no authentication.
- **Plain-text Credentials**: Stored in NVS without encryption.
- **Single Sensor**: Supports one AS3935 sensor per device.

## Security Recommendations

1. **Network Isolation**: Restrict device access to trusted networks.
2. **Credential Encryption**: Implement for production deployments.
3. **API Authentication**: Add optional auth layer if exposing externally.

## Deployment Checklist

```bash
# 1. Full clean build and test
idf.py fullclean
idf.py -DTEST=true build

# 2. Build production firmware
idf.py build

# 3. Flash and verify on test device
idf.py -p /dev/ttyUSB0 flash monitor

# 4. Test all functionality
# - Web UI provisioning
# - MQTT connection and events
# - REST API endpoints
# - Settings persistence

# 5. Tag release in git
git tag -a v1.0.0 -m "Initial stable release"
git push origin v1.0.0
```

## Version History

### v1.0.0 (November 2025)

**Initial Production Release**
- Complete lightning detection system
- Web UI for provisioning and configuration
- REST API with 20+ endpoints
- MQTT event publishing
- Automatic sensor calibration
- NTP time synchronization
- Event streaming via SSE
- Unit tests for core functionality

## Support

For issues, feedback, or contributions:
- Open a GitHub issue
- Check [GETTING_STARTED.md](GETTING_STARTED.md) for setup help
- Review [API_REFERENCE.md](API_REFERENCE.md) for endpoint details
- Consult [README.md](README.md) troubleshooting section

