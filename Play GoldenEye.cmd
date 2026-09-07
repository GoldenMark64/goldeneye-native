@echo off
setlocal
set "GAME_DIR=%~dp0getv\build-windows"
set "GAME=%GAME_DIR%\goldeneye.exe"

if not exist "%GAME%" (
  echo GoldenEye-Native has not finished building yet.
  echo Run GoldenEye-Native-Setup.exe again to resume setup.
  pause
  exit /b 1
)

start "" /d "%GAME_DIR%" "%GAME%" --launcher
