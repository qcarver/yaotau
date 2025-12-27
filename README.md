# yaota_demo (ESP-IDF 5.2.1)

This is a minimal ESP-IDF project that vendors a custom component **components/yaotau**.

## What it does
- `ota_helper_init()` runs at startup (non-fatal).
- Connects to Wi-Fi using Kconfig values.
- Fetches `version.json` from your server.
- Compares versions (semver `x.y.z`).
- If server version is newer, downloads `the_app_image.bin` and reboots into the other OTA slot.

## Quick start
1. Install / export ESP-IDF v5.2.1 environment.
2. Configure:
   - `idf.py menuconfig`
   - `yaotau (yet another over-the-air updater)`:
     - WiFi SSID / Password
     - Server version URL (points to version.json)
3. Build & flash:
   - `idf.py build`
   - `idf.py flash monitor`

## Server-side layout expected
Your server should expose:
- `version.json` containing:
  ```json
  { "version": "1.0.3", "image_url": "http://<host>:<port>/the_app_image.bin" }
  ```
- `the_app_image.bin` (the built app image)

## Version bump behavior
This demo bumps the PATCH number (the 3rd tuple) on *every* build invocation.
It does so via `tools/yaota_bump_version.py`, which updates `version.txt` and generates
`build/generated/yaota_version.h`.

The app prints both:
- ESP-IDF app descriptor version (`esp_app_get_description()->version`)
- yaotau build version (`YAOTA_BUILD_VERSION`)
