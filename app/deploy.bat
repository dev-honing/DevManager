@echo off
rem  Build Release and stage a self-contained folder in ..\dist\DevManager
rem  (devmanager-gui.exe + devmanager-scan.exe + bundled Qt runtime + config\).
rem  Copy that folder to a new PC and run it -- no Qt install needed.
setlocal
cd /d "%~dp0"

set DIST=%~dp0..\dist\DevManager

cmake -S . -B build -DDEVMANAGER_BUILD_TESTS=OFF || exit /b 1
cmake --build build --config Release || exit /b 1
cmake --install build --config Release --prefix "%DIST%" || exit /b 1

echo.
echo Deployed to %DIST%
