@echo off
setlocal
set "ROOT=%~dp0"
set "SRC=%ROOT%..\src"
set "GXX=F:\Tools\App\mingw64\bin\g++.exe"
if not exist "%GXX%" set "GXX=g++"

"%GXX%" -std=c++17 -O2 -municode -DWIN32 -DUNICODE -D_UNICODE ^
  -I"%SRC%\sdk" "%ROOT%loadtest.cpp" ^
  -o "%ROOT%loadtest.exe"
if errorlevel 1 (
  echo LOADTEST BUILD FAILED
  exit /b 1
)
echo LOADTEST BUILD OK
