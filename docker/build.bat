@echo off
rem  Build the three dev images. Tags match config\project-types.json.
setlocal
cd /d "%~dp0"
docker build -t ai-dev-base:0.1 base   || exit /b 1
docker build -t ai-dev-cpp:0.1  cpp    || exit /b 1
docker build -t ai-dev-next:0.1 nextjs || exit /b 1
echo.
echo built: ai-dev-base:0.1  ai-dev-cpp:0.1  ai-dev-next:0.1
