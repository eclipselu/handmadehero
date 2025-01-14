@echo off
setlocal

where /q cl || (
  call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
)
set code_dir=%~dp0%
set build_dir=%code_dir%\..\build

if not exist %build_dir% mkdir %build_dir%
pushd %build_dir%
cl /nologo /MT /GR- /EHa- /Od /Oi /WX /W4 /wd4201 /wd4189 /wd4100 /D BUILD_DEBUG=1 -FC -Z7 %code_dir%\win32_handmade.cpp user32.lib Gdi32.lib Xinput.lib
:: clang -DBUILD_DEBUG=1 -g -o win32_handmade.exe %code_dir%\win32_handmade.cpp -luser32 -lGdi32 -lXinput.lib
popd
