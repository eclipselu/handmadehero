@echo off
set code_dir=%~dp0%
set build_dir=%code_dir%\..\build
mkdir %build_dir%
pushd %build_dir%
cl -FC -Zi %code_dir%\win32_handmade.cpp user32.lib Gdi32.lib
popd
