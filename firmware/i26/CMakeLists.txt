cmake_minimum_required(VERSION 3.13)

# This directory is intentionally self-contained as the distributable firmware
# source folder. Point PICO_SDK_PATH at a Raspberry Pi Pico SDK checkout.
if(NOT DEFINED PICO_SDK_PATH OR PICO_SDK_PATH STREQUAL "")
    if(DEFINED ENV{PICO_SDK_PATH} AND NOT "$ENV{PICO_SDK_PATH}" STREQUAL "")
        set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
    else()
        message(FATAL_ERROR
            "PICO_SDK_PATH is required. Set -DPICO_SDK_PATH=/path/to/pico-sdk "
            "or define the PICO_SDK_PATH environment variable.")
    endif()
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")

project(i26 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

# Eigen can be supplied locally for offline/reproducible builds. If omitted,
# this standalone firmware folder fetches the same fixed Eigen revision used by CI.
set(EIGEN3_INCLUDE_DIR "" CACHE PATH "Directory containing Eigen/Dense")

if(NOT EIGEN3_INCLUDE_DIR)
    include(FetchContent)
    FetchContent_Declare(
        eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG 3147391d946bb4b6c68edd901f2add6ac1f31f8c
        GIT_SHALLOW FALSE
    )
    FetchContent_GetProperties(eigen)
    if(NOT eigen_POPULATED)
        FetchContent_Populate(eigen)
    endif()
    set(EIGEN3_INCLUDE_DIR "${eigen_SOURCE_DIR}")
endif()

if(NOT EXISTS "${EIGEN3_INCLUDE_DIR}/Eigen/Dense")
    message(FATAL_ERROR "Eigen/Dense was not found at ${EIGEN3_INCLUDE_DIR}")
endif()

add_executable(i26
    pico_copter.cpp
    sensor.cpp
    ekf.cpp
    lsm9ds1_reg.c
    pwm.cpp
    radio.cpp
    control.cpp
    VL53L1X_api.c
    vl53l1_platform.c
    rgbled.cpp
    WS2812.cpp
)

target_include_directories(i26 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${EIGEN3_INCLUDE_DIR}"
)

target_link_libraries(i26 PRIVATE
    hardware_spi
    hardware_pwm
    hardware_uart
    hardware_i2c
    hardware_pio
    pico_stdlib
    pico_multicore
)

pico_enable_stdio_usb(i26 1)
pico_enable_stdio_uart(i26 0)
pico_add_extra_outputs(i26)
pico_generate_pio_header(i26
    ${CMAKE_CURRENT_LIST_DIR}/WS2812.pio
)
