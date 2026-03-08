@echo off
setlocal

rem ---- Pfade anpassen falls nötig ----
set OPENOCD_BIN=E:\Selfbus\xpack-openocd-0.12.0-7\bin\openocd.exe
set OPENOCD_SCRIPTS=E:\Selfbus\xpack-openocd-0.12.0-7\openocd\scripts

rem ---- Pfad zur OpenOCD-Konfig relativ zum Script ----
set SCRIPT_DIR=%~dp0
set OPENOCD_CFG=%SCRIPT_DIR%..\openocd\sbdap_pico2.cfg

echo.
echo Starting OpenOCD with SBDAP + Pico2 configuration
echo.

if not exist "%OPENOCD_BIN%" (
  echo ERROR: openocd.exe not found at "%OPENOCD_BIN%"
  pause
  exit /b 1
)

if not exist "%OPENOCD_SCRIPTS%\target\rp2350.cfg" (
  echo ERROR: rp2350.cfg not found in "%OPENOCD_SCRIPTS%\target"
  pause
  exit /b 1
)

if not exist "%OPENOCD_CFG%" (
  echo ERROR: OpenOCD config not found at "%OPENOCD_CFG%"
  pause
  exit /b 1
)

echo Using OpenOCD: %OPENOCD_BIN%
echo Using scripts: %OPENOCD_SCRIPTS%
echo Using config : %OPENOCD_CFG%
echo.

"%OPENOCD_BIN%" -s "%OPENOCD_SCRIPTS%" -f "%OPENOCD_CFG%"

echo.
echo OpenOCD exited.
pause