cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)

project(humidity C CXX ASM)
set(CMAKE_C_STANDARD 23)

set(PICO_BOARD "pico_w")
pico_sdk_init()

add_compile_options(-Wall -pedantic)

add_executable(humidity sht4x.c)
include_directories(humidity PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_add_extra_outputs(humidity)
target_link_libraries(humidity pico_cyw43_arch_lwip_poll pico_stdlib hardware_i2c pico_lwip_mdns)

target_compile_definitions(humidity PRIVATE WLAN_SSID="${wlan_ssid}")
target_compile_definitions(humidity PRIVATE WLAN_PASS="${wlan_pass}")
target_compile_definitions(humidity PRIVATE CYW43_HOST_NAME="${hostname}")
target_compile_definitions(humidity PRIVATE MDNS_SERVICE_NAME="${servicename}")

add_executable(sht3x sht3x.c)
include_directories(sht3x PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_add_extra_outputs(sht3x)
target_link_libraries(sht3x pico_cyw43_arch_lwip_poll pico_stdlib hardware_i2c pico_lwip_mdns)

target_compile_definitions(sht3x PRIVATE WLAN_SSID="${wlan_ssid}")
target_compile_definitions(sht3x PRIVATE WLAN_PASS="${wlan_pass}")
target_compile_definitions(sht3x PRIVATE CYW43_HOST_NAME="${hostname}")
target_compile_definitions(sht3x PRIVATE MDNS_SERVICE_NAME="${servicename}")



add_executable(bme bme688.c)
include_directories(bme PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_add_extra_outputs(bme)
target_link_libraries(bme pico_cyw43_arch_lwip_poll pico_stdlib hardware_i2c pico_lwip_mdns)

target_compile_definitions(bme PRIVATE WLAN_SSID="${wlan_ssid}")
target_compile_definitions(bme PRIVATE WLAN_PASS="${wlan_pass}")
target_compile_definitions(bme PRIVATE CYW43_HOST_NAME="${hostname}")
target_compile_definitions(bme PRIVATE MDNS_SERVICE_NAME="${servicename}")

