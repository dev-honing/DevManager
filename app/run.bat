@echo off
REM Launch the built GUI with Qt on PATH. Edit QTBIN if your Qt lives elsewhere.

setlocal
cd /d "%~dp0"

set "QTBIN=C:\Qt\6.10.3\msvc2022_64\bin"
if not exist "%QTBIN%\Qt6Core.dll" (
  echo Qt not found at %QTBIN% - edit QTBIN in run.bat
  exit /b 1
)
set "PATH=%QTBIN%;%PATH%"

if not exist "build\Debug\devmanager-gui.exe" (
  echo Not built yet - run build.bat first
  exit /b 1
)
start "" "build\Debug\devmanager-gui.exe"
