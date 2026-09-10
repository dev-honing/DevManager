@echo off
REM Configure + build DevManager. Run from the app\ folder.
REM Requires: Visual Studio 2026 (or Build Tools), CMake >= 3.24, Qt 6.10 msvc2022_64.

setlocal
cd /d "%~dp0"

cmake --preset windows-v143 || goto :err
cmake --build build --config Debug || goto :err

echo.
echo Build OK.
echo   GUI  : build\Debug\devmanager-gui.exe   (use run.bat so Qt DLLs resolve)
echo   CLI  : build\Debug\devmanager-scan.exe
echo   VS   : open build\DevManager.sln, set devmanager-gui as startup project
exit /b 0

:err
echo.
echo Build FAILED.
exit /b 1
