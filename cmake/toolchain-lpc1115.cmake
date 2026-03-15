set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m0)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MCPU_FLAGS "-mcpu=cortex-m0 -mthumb")

set(CMAKE_C_FLAGS "${MCPU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS "${MCPU_FLAGS} -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections")

set(CMAKE_EXE_LINKER_FLAGS
        "${MCPU_FLAGS} -Wl,--gc-sections --specs=nosys.specs"
)
