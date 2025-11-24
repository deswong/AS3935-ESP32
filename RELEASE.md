AS3935-ESP32 — Release checklist (v0.1)

This file collects a minimal checklist and guidance for the initial public release.

Before publishing
- [ ] Run a full clean build and ensure all unit tests pass:

```bash
idf.py fullclean
idf.py -DTEST=true build
```

- [ ] Run the integration build and flash a test device:

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

- [ ] Manual runtime validation (on a test device):
  - Connect to the device AP and verify the captive-portal SPA appears.
  - Provision Wi‑Fi and MQTT settings via the SPA and confirm the device connects.
  - Confirm SNTP sync occurs after Wi‑Fi connect (check serial logs for "Initializing SNTP" and a sane date/time).
  - Verify events are published to the MQTT broker and payload contains `uptime_ms` and `time_ok` (and `ts_ms` when synced).

Repository housekeeping
- [ ] Ensure generated headers (build/...) are not committed.
- [ ] Tag the release (git tag -a v0.1 -m "Initial release")
- [ ] Update CHANGELOG.md with notable changes (if present)

Optional improvements for next release
- Make NTP server(s) configurable via web UI and persist to NVS.
- Expose `time_ok` and last SNTP sync time via REST endpoints.
- Add integration tests for provisioning flow (requires hardware-in-the-loop or simulated networking).

