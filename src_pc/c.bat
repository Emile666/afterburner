echo on
REM path to your Win64 cross-compiler
set PATH=%PATH%;d:\mingw32\bin

i686-w64-mingw32-gcc -g3 -O0  -o afterburner.exe afterburner.c aftb_jtag.c serial_port.c -D_USE_WIN_API_ -DNO_CLOSE