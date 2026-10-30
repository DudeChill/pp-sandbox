@echo off
setlocal
cd /d "%~dp0"
cmake -S . -B build -A x64 || exit /b 1
cmake --build build --config Release -j || exit /b 1
build\Release\pp.exe %*
