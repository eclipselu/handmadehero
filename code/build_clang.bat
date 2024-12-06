@echo off
set code_dir=%~dp0%
set build_dir=%code_dir%\..\build
mkdir %build_dir%
pushd %build_dir%
clang++ %code_dir%\win32_handmade.cpp -g -o win32_handmade.exe -luser32 -lGdi32
popd
