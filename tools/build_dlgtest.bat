@echo off
setlocal
set "ROOT=%~dp0"
set "SRC=%ROOT%..\src"
set "GXX=F:\Tools\App\mingw64\bin\g++.exe"
if not exist "%GXX%" set "GXX=g++"

"%GXX%" -std=c++17 -O2 -municode -DWIN32 -DUNICODE -D_UNICODE ^
  -I"%SRC%\core" -I"%SRC%\net" -I"%SRC%\router" -I"%SRC%\ui" -I"%SRC%\sdk" ^
  "%ROOT%dlgtest.cpp" "%SRC%\ui\OptionsDlg.cpp" "%SRC%\core\DisplayConfig.cpp" ^
  "%SRC%\core\PluginIcon.cpp" ^
  -luser32 -lgdi32 -lshell32 ^
  -o "%ROOT%dlgtest.exe"
if errorlevel 1 (
  echo DLGTEST BUILD FAILED
  exit /b 1
)
echo DLGTEST BUILD OK
