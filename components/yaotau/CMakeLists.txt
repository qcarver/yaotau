
idf_component_register(
  SRCS
    "src/ota_helper.c"
    "src/yaotau_wifi.c"
    "src/yaotau_http.c"
    "src/yaotau_semver.c"
  INCLUDE_DIRS "include"
  REQUIRES
    nvs_flash
    esp_event
    esp_netif
    esp_wifi
    esp_http_client
    app_update
    cjson
)

# Get build properties and use them directly
idf_build_get_property(YAOTAU_PROJECT_NAME APP_NAME)
idf_build_get_property(YAOTAU_BUILD_VERSION APP_VERSION)

message(STATUS "Yaotau component received: APP_NAME='${YAOTAU_PROJECT_NAME}', APP_VERSION='${YAOTAU_BUILD_VERSION}'")

target_compile_definitions(${COMPONENT_LIB} PRIVATE 
    YAOTAU_PROJECT_NAME="${YAOTAU_PROJECT_NAME}"
    YAOTAU_BUILD_VERSION="${YAOTAU_BUILD_VERSION}"
)

#insert a message here to output what 
message(STATUS "Yaotau component: YAOTAU_PROJECT_NAME='${YAOTAU_PROJECT_NAME}', YAOTAU_BUILD_VERSION='${YAOTAU_BUILD_VERSION}'")  
