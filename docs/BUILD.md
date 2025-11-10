# Building the firmware (local)

This project uses ESP-IDF. The CI workflow will attempt a build on GitHub Actions. To build locally on Linux (developer machine):

1. Install ESP-IDF (recommended version 5.0). Follow the official instructions:
   https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/installation/index.html

2. In a shell (assuming your ESP-IDF is installed to $IDF_PATH and you've run the required export script):

```bash
cd /path/to/AS3935-ESP32
idf.py set-target esp32c3
idf.py build
```

## Docker-based build (no local ESP-IDF install)

If you don't have ESP-IDF locally, use the provided Docker helper which runs the official Espressif image:

```bash
./scripts/docker_build.sh
```

This will mount the project into the container and run `idf.py set-target esp32c3 && idf.py build`.

## Vendored cJSON

This repository includes a minimal cJSON shim under `components/cjson_shim` that provides the small subset of JSON parsing functionality used by this project. The shim avoids requiring the full esp-cjson component and keeps CI simpler.

Note: The shim supports only small, flat JSON objects and basic types (strings, numbers, booleans). If you later need full JSON semantics, replace `components/cjson_shim` with the upstream `esp-cjson` implementation or otherwise add a full JSON component.

```bash
./scripts/vendor_esp_cjson.sh
```

This clones the official `esp-cjson` project into `components/esp_cjson`. After that, rebuild.

3. If the build fails on GitHub Actions, check the Actions tab for the full log. Locally, read the build output for missing components or include path issues.

Notes:
- This repository does not include the ESP-IDF toolchain. Use the `esp-idf/setup-esp-idf` GitHub Action (already added) for CI builds.
- If you want, I can add a Dockerfile or fully pinned CI (toolchain versions) for reproducible builds — tell me if you'd like that.

## Building & running unit tests (Unity)

This repository includes Unity-based unit tests built into the `as3935_test` component.

1. Build test artifacts:

```bash
idf.py -DTEST=true build
```

2. Flash and run tests on a device (the test runner prints Unity results on serial):

```bash
idf.py -DTEST=true -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with your serial device. Tests will run automatically on startup and output results to the serial monitor.

Test helpers
- `as3935_validate_and_maybe_restore(baseline_sp, baseline_li, duration_s)` — synchronous helper to exercise validation logic in unit tests.
- `as3935_test_set_counters(sp, li)` — set the IRQ-driven counters used by calibration/validation routines for deterministic tests.

If you want CI to run these tests automatically, we can add a GitHub Actions job that uses a QEMU target or a self-hosted runner with connected hardware; that is a larger change and I can propose it if desired.
