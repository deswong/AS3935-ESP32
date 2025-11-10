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

This repository now includes a minimal vendored `esp_cjson` component under `components/esp_cjson` to make local builds more robust. The GitHub Actions CI also clones `esp-cjson` into `components/esp_cjson` before building.

Note: The vendored copy here is a minimal stub to satisfy builds in environments missing the real `esp-cjson`. For full JSON parsing support, replace `components/esp_cjson` with the upstream esp-cjson implementation (or remove it and rely on the cJSON component in your ESP-IDF installation).

To vendor the real upstream esp-cjson into the repo, run:

```bash
./scripts/vendor_esp_cjson.sh
```

This clones the official `esp-cjson` project into `components/esp_cjson`. After that, rebuild.

3. If the build fails on GitHub Actions, check the Actions tab for the full log. Locally, read the build output for missing components or include path issues.

Notes:
- This repository does not include the ESP-IDF toolchain. Use the `esp-idf/setup-esp-idf` GitHub Action (already added) for CI builds.
- If you want, I can add a Dockerfile or fully pinned CI (toolchain versions) for reproducible builds — tell me if you'd like that.
