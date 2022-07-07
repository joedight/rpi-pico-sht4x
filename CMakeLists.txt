cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)

project(humidity C CXX ASM)
set(CMAKE_C_STANDARD 17)

set(PICO_BOARD "pico_w")
pico_sdk_init()

add_executable(humidity main.c)
target_include_directories(humidity PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_add_extra_outputs(humidity)
target_link_libraries(humidity pico_cyw43_arch_lwip_poll pico_stdlib hardware_i2c pico_lwip_mdns)

target_compile_definitions(humidity PRIVATE WLAN_SSID="${wlan_ssid}")
target_compile_definitions(humidity PRIVATE WLAN_PASS="${wlan_pass}")
target_compile_definitions(humidity PRIVATE CYW43_HOST_NAME="${hostname}")
target_compile_definitions(humidity PRIVATE MDNS_SERVICE_NAME="${servicename}")

