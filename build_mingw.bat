@echo off
setlocal
REM Build RouterTotalSpeed.dll with MinGW-w64 (x64, statically linked)
REM Default toolchain: F:\Tools\App\mingw64 ; falls back to tools in PATH

set "ROOT=%~dp0"
set "SRC=%ROOT%src"
set "OUTDIR=%ROOT%bin\Release\x64"
set "GXX=F:\Tools\App\mingw64\bin\g++.exe"
if not exist "%GXX%" set "GXX=g++"
set "WINDRES=F:\Tools\App\mingw64\bin\windres.exe"
if not exist "%WINDRES%" set "WINDRES=windres"

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM Compile icon resource (src/ui/RouterTotalSpeed.rc -> app.ico)
"%WINDRES%" -i "%SRC%\ui\RouterTotalSpeed.rc" -I "%SRC%\ui" -O coff -o "%OUTDIR%\RouterTotalSpeed_res.o"
if errorlevel 1 (
  echo RESOURCE COMPILE FAILED
  exit /b 1
)

REM -Os + LTO + 段级裁剪：静态 libstdc++ 的体积优化（不改运行库依赖）
"%GXX%" -std=c++17 -Os -flto -fno-exceptions -fno-rtti ^
  -ffunction-sections -fdata-sections -Wl,--gc-sections ^
  -DNDEBUG -DWIN32 -DUNICODE -D_UNICODE ^
  -shared -static -static-libgcc -static-libstdc++ -s ^
  -I"%SRC%\core" -I"%SRC%\net" -I"%SRC%\router" -I"%SRC%\ui" -I"%SRC%\sdk" ^
  "%SRC%\core\RouterTotalSpeedPlugin.cpp" "%SRC%\core\DisplayConfig.cpp" ^
  "%SRC%\core\PluginIcon.cpp" "%SRC%\core\pch.cpp" ^
  "%SRC%\net\Util.cpp" "%SRC%\net\Http.cpp" ^
  "%SRC%\router\UpnpWan.cpp" ^
  "%SRC%\ui\OptionsDlg.cpp" ^
  "%OUTDIR%\RouterTotalSpeed_res.o" ^
  -lwinhttp -luser32 -lgdi32 -lshell32 -lws2_32 -liphlpapi ^
  -o "%OUTDIR%\RouterTotalSpeed.dll"

if errorlevel 1 (
  echo BUILD FAILED
  exit /b 1
)
echo BUILD OK: %OUTDIR%\RouterTotalSpeed.dll
