#!/bin/bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <project-root>" >&2
    exit 1
fi

PROJECT_ROOT=$(cd "$1" && pwd)
CMAKE_FILE="$PROJECT_ROOT/CMakeLists.txt"

APP_NAME=$(sed -n 's/.*idf_build_set_property(APP_NAME[[:space:]]*"\([^"]*\)".*/\1/p' "$CMAKE_FILE" | head -n 1)
if [[ -z "$APP_NAME" || "$APP_NAME" == '$'{* ]]; then
    APP_NAME=$(sed -n 's/.*set(APP_NAME[[:space:]]*"\([^"]*\)".*/\1/p' "$CMAKE_FILE" | head -n 1)
fi
if [[ -z "$APP_NAME" ]]; then
    APP_NAME=$(sed -n 's/.*project(\([^[:space:])]*\).*/\1/p' "$CMAKE_FILE" | head -n 1)
fi

if [[ -z "$APP_NAME" ]]; then
    echo "ERROR: Could not determine APP_NAME from $CMAKE_FILE" >&2
    exit 1
fi

/bin/bash "$PROJECT_ROOT/components/yaotau/tools/update_ota_payload.sh" "$PROJECT_ROOT"

SERVE_DIR="$PROJECT_ROOT/serve_${APP_NAME}"
cd "$SERVE_DIR"

echo "Serving OTA files from $SERVE_DIR on port 8070"
python3 -m http.server 8070