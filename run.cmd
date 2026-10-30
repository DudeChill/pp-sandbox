@echo off
setlocal
cd /d "%~dp0"
if not defined VCPKG_ROOT if exist "%USERPROFILE%\vcpkg\vcpkg.exe" set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
if not defined VCPKG_ROOT (
  echo VCPKG_ROOT is not set.
  echo Install vcpkg, run "vcpkg install sdl2 --triplet x64-windows", and set VCPKG_ROOT.
  exit /b 1
)
cmake --preset windows || exit /b 1
cmake --build --preset windows || exit /b 1
build\windows\Release\pp.exe %*
