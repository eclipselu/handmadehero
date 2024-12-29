@echo off
setlocal

where /q cl || (
  call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
)
set code_dir=%~dp0%
set build_dir=%code_dir%\..\build

if not exist %build_dir% mkdir %build_dir%
pushd %build_dir%
cl -FC -Zi %code_dir%\win32_handmade.cpp user32.lib Gdi32.lib Xinput.lib
popd
