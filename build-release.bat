@echo off
echo ================================
echo KeynetikPOS Build Script
echo ================================

REM ---- CONFIG ----
set QT_PATH=C:\Qt\6.11.0\mingw_64
set BUILD_DIR=build

REM ---- Load Qt Environment ----
call %QT_PATH%\bin\qtenv2.bat

REM ---- Clean old build ----
if exist %BUILD_DIR% (
    echo Cleaning old build...
    rmdir /s /q %BUILD_DIR%
)

REM ---- Configure ----
echo Configuring project...
cmake -B %BUILD_DIR% -S . -G "MinGW Makefiles" ^
-DCMAKE_PREFIX_PATH=%QT_PATH%

if %errorlevel% neq 0 exit /b

REM ---- Build ----
echo Building...
cmake --build %BUILD_DIR%

if %errorlevel% neq 0 exit /b

REM ---- Go to build dir ----
cd %BUILD_DIR%

REM ---- Deploy Qt DLLs ----
echo Running windeployqt...
"%QT_PATH%\bin\windeployqt.exe" KeynetikPOS.exe

echo.
echo ================================
echo BUILD COMPLETE ✅
echo ================================

pause