@echo off
setlocal

rem ---- Pfade anpassen falls nötig ----
set GDB_BIN=C:\Program Files\Raspberry Pi\Pico SDK v1.5.1\gcc-arm-none-eabi\bin\arm-none-eabi-gdb.exe
set ELF=E:\Selfbus\Selfbus Git\theport\cmake-build-debug\bringup_blinky_uart.elf

echo.
echo Starting GDB for Pico2 debugging
echo.

if not exist "%GDB_BIN%" (
  echo ERROR: arm-none-eabi-gdb.exe not found
  pause
  exit /b 1
)

if not exist "%ELF%" (
  echo ERROR: ELF file not found
  pause
  exit /b 1
)

"%GDB_BIN%" ^
  -ex "target extended-remote localhost:3333" ^
  -ex "monitor reset halt" ^
  -ex "load" ^
  -ex "thbreak main" ^
  -ex "continue" ^
  "%ELF%"

echo.
