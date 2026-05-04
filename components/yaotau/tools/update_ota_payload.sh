#!/bin/bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <project-root>" >&2
    exit 1
fi

PROJECT_ROOT=$(cd "$1" && pwd)
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
YAOTAU_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

CMAKE_FILE="$PROJECT_ROOT/CMakeLists.txt"
SDKCONFIG_FILE="$PROJECT_ROOT/sdkconfig"
KCONFIG_FILE="$YAOTAU_ROOT/Kconfig"

if [[ ! -f "$CMAKE_FILE" ]]; then
    echo "ERROR: Missing CMakeLists.txt at $CMAKE_FILE" >&2
    exit 1
fi

extract_first_match() {
    local pattern="$1"
    local file_path="$2"

    sed -n "$pattern" "$file_path" | head -n 1
}

APP_NAME=$(extract_first_match 's/.*idf_build_set_property(APP_NAME[[:space:]]*"\([^"]*\)".*/\1/p' "$CMAKE_FILE")
if [[ -z "$APP_NAME" || "$APP_NAME" == '$'{* ]]; then
    APP_NAME=$(extract_first_match 's/.*set(APP_NAME[[:space:]]*"\([^"]*\)".*/\1/p' "$CMAKE_FILE")
fi
if [[ -z "$APP_NAME" ]]; then
    APP_NAME=$(extract_first_match 's/.*project(\([^[:space:])]*\).*/\1/p' "$CMAKE_FILE")
fi

APP_VERSION=$(extract_first_match 's/.*set(APP_VERSION[[:space:]]*"\([^"]*\)".*/\1/p' "$CMAKE_FILE")

if [[ -z "$APP_NAME" || -z "$APP_VERSION" ]]; then
    echo "ERROR: Could not determine APP_NAME or APP_VERSION from $CMAKE_FILE" >&2
    exit 1
fi

SERVER_URL=""
if [[ -f "$SDKCONFIG_FILE" ]]; then
    SERVER_URL=$(grep '^CONFIG_YAOTAU_SERVER_VERSION_URL=' "$SDKCONFIG_FILE" | sed 's/^CONFIG_YAOTAU_SERVER_VERSION_URL="\(.*\)"$/\1/' || true)
fi

if [[ -z "$SERVER_URL" && -f "$KCONFIG_FILE" ]]; then
    SERVER_URL=$(grep -A1 'config YAOTAU_SERVER_VERSION_URL' "$KCONFIG_FILE" | grep 'default' | sed 's/.*"\(.*\)".*/\1/' || true)
fi

if [[ -z "$SERVER_URL" ]]; then
    echo "ERROR: Could not determine YAOTAU server URL from sdkconfig or Kconfig." >&2
    exit 1
fi

BASE_URL=${SERVER_URL%/version.json}
if ! echo "$BASE_URL" | grep -Eq '^https?://[^/]+$'; then
    echo "ERROR: Derived BASE_URL is not absolute: '$BASE_URL'" >&2
    echo "Expected something like: http://192.168.1.98:8070" >&2
    exit 1
fi

BUILD_BIN="$PROJECT_ROOT/build/${APP_NAME}.bin"
if [[ ! -f "$BUILD_BIN" ]]; then
    echo "ERROR: Missing built binary at $BUILD_BIN" >&2
    echo "Run 'idf.py build' first." >&2
    exit 1
fi

SERVE_DIR="$PROJECT_ROOT/serve_${APP_NAME}"
SERVE_BIN="$SERVE_DIR/${APP_NAME}.bin"
UPDATE_WRAPPER="$SERVE_DIR/update.sh"
START_WRAPPER="$SERVE_DIR/start.sh"

mkdir -p "$SERVE_DIR"
cp "$BUILD_BIN" "$SERVE_BIN"

cat > "$SERVE_DIR/version.json" <<EOF
{
  "version": "$APP_VERSION",
  "image_url": "${BASE_URL}/${APP_NAME}.bin"
}
EOF

cat > "$UPDATE_WRAPPER" <<EOF
#!/bin/bash
set -euo pipefail

PROJECT_ROOT=\$(cd "\$(dirname "\$0")/.." && pwd)
exec /bin/bash "\$PROJECT_ROOT/components/yaotau/tools/update_ota_payload.sh" "\$PROJECT_ROOT"
EOF

cat > "$START_WRAPPER" <<EOF
#!/bin/bash
set -euo pipefail

PROJECT_ROOT=\$(cd "\$(dirname "\$0")/.." && pwd)
exec /bin/bash "\$PROJECT_ROOT/components/yaotau/tools/start_ota_server.sh" "\$PROJECT_ROOT"
EOF

chmod +x "$UPDATE_WRAPPER" "$START_WRAPPER"

echo "Updated OTA payload:"
echo "  app: $APP_NAME"
echo "  version: $APP_VERSION"
echo "  serve dir: $SERVE_DIR"
echo "  base url: $BASE_URL"