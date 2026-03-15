cmake_minimum_required(VERSION 3.13)

# Prefer pico-sdk submodule if present
set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/extern/pico-sdk")

# Fallback: allow external install via env/variable
if (NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    if (NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    endif ()
endif ()

if (NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR "pico-sdk not found. Expected at extern/pico-sdk or set PICO_SDK_PATH.")
endif ()

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
        sblib/src/rp2354/digital_pin.cpp
        platform/rp2354/src/sb_iap_compat.cpp
)

target_include_directories(sblib_platform PUBLIC
        platform/rp2354/include
        sblib/inc
)

target_compile_definitions(sblib_platform PUBLIC
        SBLIB_PLATFORM_RP2354
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
        sblib/src/arrays.cpp
        sblib/src/buffered_stream.cpp
        sblib/src/ioports.cpp
        sblib/src/mem_mapper.cpp
        sblib/src/onewire.cpp
        sblib/src/new.cpp
        sblib/src/print.cpp
        sblib/src/stream.cpp
        sblib/src/timer.cpp
        sblib/src/version.cpp
        sblib/src/utils.cpp
        sblib/src/serial.cpp
        sblib/src/debounce.cpp
        sblib/src/i2c.cpp
        sblib/src/eib/bus.cpp
        sblib/src/eib/bus_debug.cpp
        sblib/src/eib/apci.cpp
        sblib/src/eib/datapoint_types.cpp
        sblib/src/eib/knx_tlayer4.cpp
        sblib/src/eib/bcu_base.cpp
        sblib/src/eib/bcu_default.cpp
        sblib/src/eib/bcu1.cpp
        sblib/src/eib/userRam.cpp
        sblib/src/eib/userEeprom.cpp
        sblib/src/eib/com_objects.cpp
        sblib/src/eib/com_objectsBCU1.cpp
        sblib/src/eib/addr_tables.cpp
        sblib/src/eib/addr_tablesBCU1.cpp
        sblib/src/eib/memory.cpp
        platform/rp2354/src/sb_iap_compat.cpp
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
        hardware_i2c
        hardware_pwm
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

add_executable(test_app
        src/main.cpp
)

target_link_libraries(test_app
        sblib_core
        sblib_platform
)

pico_enable_stdio_usb(test_app 0)

option(BUILD_LPC_APP "Build LPC1115 firmware" OFF)

if(BUILD_LPC_APP)

    enable_language(C CXX ASM)

    add_executable(lpc_app
            src/app_main.cpp
            src/cr_startup_lpc11xx.cpp
            extern/CMSIS_CORE_LPC11xx/src/system_LPC11xx.c
    )

    target_include_directories(lpc_app PRIVATE
            sblib/inc
            extern/CMSIS_CORE_LPC11xx/inc
    )

    target_compile_definitions(lpc_app PRIVATE
            __LPC11XX__
    )

    target_link_options(lpc_app PRIVATE
            -T${CMAKE_SOURCE_DIR}/cmake/lpc1115.ld
            -Wl,-Map=${CMAKE_BINARY_DIR}/lpc_app.map
    )

    target_link_libraries(lpc_app
            sblib_core
            sblib_platform
    )

endif()
