#!/usr/bin/env bash
# Simple helper to build the firmware inside Espressif's official Docker image.
# Usage: ./scripts/docker_build.sh
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="espressif/idf:release-v5.0"

echo "Using image: $IMAGE"

docker run --rm -v "$ROOT_DIR":/project -w /project -u $(id -u):$(id -g) $IMAGE /bin/bash -lc "idf.py set-target esp32c3 && idf.py build"
