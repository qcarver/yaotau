\
cmake_minimum_required(VERSION 3.16)

# ---- yaotau: read current version from version.txt at configure time (best-effort) ----
file(READ "${CMAKE_SOURCE_DIR}/version.txt" YAOTA_VER_TXT)
string(STRIP "${YAOTA_VER_TXT}" YAOTA_VER_TXT)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# PROJECT_VER is used by ESP-IDF app descriptor (may lag if you don't reconfigure each build).
# We still set it from version.txt for sanity, but the authoritative build-time version is
# YAOTA_BUILD_VERSION generated per-build (see bump target below).
set(PROJECT_VER "${YAOTA_VER_TXT}")

project(yaota_demo)

# ---- yaotau: bump PATCH every build + generate header in build/generated/yaota_version.h ----
set(YAOTA_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
set(YAOTA_VER_H   "${YAOTA_GEN_DIR}/yaota_version.h")

add_custom_target(yaota_bump ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${YAOTA_GEN_DIR}"
  COMMAND ${PYTHON} "${CMAKE_SOURCE_DIR}/tools/yaota_bump_version.py"
          --version-file "${CMAKE_SOURCE_DIR}/version.txt"
          --out-header "${YAOTA_VER_H}"
  BYPRODUCTS "${YAOTA_VER_H}"
  COMMENT "yaotau: bumping patch version + generating yaota_version.h"
  VERBATIM
)

# Ensure app rebuilds when the generated header changes.
add_dependencies(app yaota_bump)
idf_build_set_property(INCLUDE_DIRECTORIES "${YAOTA_GEN_DIR}" APPEND)

# ---- yaotau: emit serve_me artifacts after build ----
set(SERVE_ME_DIR  "${CMAKE_BINARY_DIR}/serve_me")
set(SERVE_ME_BIN  "${SERVE_ME_DIR}/the_app_image.bin")
set(SERVE_ME_JSON "${SERVE_ME_DIR}/version.json")

# The URL in version.json should match your actual server host/port.
# Keep it simple: the component reads image_url directly from version.json.
set(YAOTA_IMAGE_URL_DEFAULT "http://192.168.1.98:8070/the_app_image.bin")

add_custom_command(TARGET app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${SERVE_ME_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:app>" "${SERVE_ME_BIN}"
  COMMAND ${PYTHON} "${CMAKE_SOURCE_DIR}/tools/yaota_write_version_json.py"
          --version-file "${CMAKE_SOURCE_DIR}/version.txt"
          --image-url "${YAOTA_IMAGE_URL_DEFAULT}"
          --out "${SERVE_ME_JSON}"
  BYPRODUCTS "${SERVE_ME_BIN}" "${SERVE_ME_JSON}"
  COMMENT "yaotau: generating build/serve_me artifacts"
  VERBATIM
)

# ---- yaotau: run a pre-flash script (update_and_start.sh) before flashing ----
# If you don't want this, delete this block.
add_custom_command(TARGET flash PRE_BUILD
  COMMAND bash -lc "if [ -f \"${SERVE_ME_DIR}/update_and_start.sh\" ]; then chmod +x \"${SERVE_ME_DIR}/update_and_start.sh\" && cd \"${SERVE_ME_DIR}\" && ./update_and_start.sh; else echo 'yaotau: (note) no update_and_start.sh in build/serve_me; skipping'; fi"
  COMMENT "yaotau: running update_and_start.sh before flash (if present)"
  VERBATIM
)
