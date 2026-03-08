cmake_minimum_required(VERSION 3.13)

# Prefer pico-sdk submodule if present
set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/extern/pico-sdk")

# Fallback: allow external install via env/variable
if (NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    if (NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    endif()
endif()

if (NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR "pico-sdk not found. Expected at extern/pico-sdk or set PICO_SDK_PATH.")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(selfbus_rp2350_bringup C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_library(sblib_platform STATIC
        platform/rp2354/src/sb_time.cpp
        platform/rp2354/src/sb_gpio.cpp
        platform/rp2354/src/sb_uart.cpp
        platform/rp2354/src/sb_flash.cpp
        platform/rp2354/src/sb_irq.cpp
        platform/rp2354/src/sb_input.cpp
        platform/rp2354/src/sb_button.cpp
        platform/rp2354/src/sb_runtime.cpp
        platform/rp2354/src/sb_event_loop.cpp
        platform/rp2354/src/sb_timer.cpp
        platform/rp2354/src/sb_cpu.cpp
)

target_include_directories(sblib_platform PUBLIC
        platform/rp2354/include
)

target_link_libraries(sblib_platform PUBLIC
        hardware_uart
        hardware_gpio
        hardware_sync
        hardware_irq
        hardware_flash
        hardware_watchdog
        pico_stdlib
        pico_time
)

add_library(sblib_core STATIC
        sblib/src/analog_pin.cpp
        sblib/src/arrays.cpp
        sblib/src/buffered_stream.cpp
        sblib/src/debounce.cpp
        sblib/src/digital_pin_shift.cpp
        sblib/src/i2c.cpp
        sblib/src/iap.cpp
        sblib/src/ioports.cpp
        sblib/src/main.cpp
        sblib/src/mem_mapper.cpp
        sblib/src/new.cpp
        sblib/src/onewire.cpp
        sblib/src/print.cpp
        sblib/src/serial.cpp
        sblib/src/serial0.cpp
        sblib/src/spi.cpp
        sblib/src/stream.cpp
        sblib/src/timer.cpp
        sblib/src/utils.cpp
        sblib/src/version.cpp

        # RP2354 backend
        sblib/src/rp2354/digital_pin.cpp
)

target_include_directories(sblib_core PUBLIC
        sblib/inc
        platform/rp2354/include
)

target_compile_definitions(sblib_core PUBLIC
        SBLIB_PLATFORM_RP2354
)

target_link_libraries(sblib_core PUBLIC
        sblib_platform
        hardware_uart
        hardware_gpio
        hardware_sync
        hardware_irq
        hardware_flash
        hardware_watchdog
        pico_stdlib
        pico_time
)

add_executable(bringup_blinky_uart
        examples/bringup_blinky_uart/main.cpp
)

target_link_libraries(bringup_blinky_uart
        sblib_core
)

pico_enable_stdio_usb(bringup_blinky_uart 1)
pico_enable_stdio_uart(bringup_blinky_uart 0)
pico_add_extra_outputs(bringup_blinky_uart)