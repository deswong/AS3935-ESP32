#!/usr/bin/env bash
# Vendor the upstream esp-cjson implementation into components/esp_cjson
# Usage: ./scripts/vendor_esp_cjson.sh
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
COMP_DIR="$ROOT_DIR/components/esp_cjson"
REPO="https://github.com/espressif/esp-cjson.git"

echo "Vendoring esp-cjson into $COMP_DIR"
if [ -d "$COMP_DIR" ]; then
  echo "Removing existing $COMP_DIR"
  rm -rf "$COMP_DIR"
fi

git clone --depth 1 "$REPO" "$COMP_DIR"

echo "Vendored esp-cjson into $COMP_DIR"

echo "Done. You can now build the project. If building with Docker use: ./scripts/docker_build.sh" 
