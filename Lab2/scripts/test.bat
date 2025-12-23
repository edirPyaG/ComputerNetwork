@echo off
REM RDT Protocol Test Script
REM Run sender and receiver with configurable parameters

setlocal EnableDelayedExpansion

REM Detect Architecture
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set ARCH=ARM64
) else if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set ARCH=x64
) else (
    set ARCH=x86
)

set BIN_DIR=bin\%ARCH%
set SENDER=%BIN_DIR%\sender.exe
set RECEIVER=%BIN_DIR%\receiver.exe

REM Check if executables exist
if not exist "%SENDER%" (
    echo ERROR: sender.exe not found in %BIN_DIR%
    echo Please run build.bat first
    pause
    exit /b 1
)

if not exist "%RECEIVER%" (
    echo ERROR: receiver.exe not found in %BIN_DIR%
    echo Please run build.bat first
    pause
    exit /b 1
)

REM Default parameters
set SEND_WIN=32
set RECV_WIN=64
set LOSS_RATE=0.05
set TEST_FILE=test_file.dat

REM Parse command line arguments
if not "%1"=="" set SEND_WIN=%1
if not "%2"=="" set LOSS_RATE=%2
if not "%3"=="" set TEST_FILE=%3

echo ========================================
echo RDT Protocol Test
echo ========================================
echo Architecture: %ARCH%
echo Send Window: %SEND_WIN% packets
echo Loss Rate: %LOSS_RATE%
echo Test File: %TEST_FILE%
echo ========================================
echo.

REM Start receiver in new window
echo Starting receiver...
start "RDT Receiver" cmd /k "%RECEIVER% %RECV_WIN%"

REM Wait for receiver to start
timeout /t 2 /nobreak >nul

REM Start sender
echo Starting sender...
"%SENDER%" %SEND_WIN% %LOSS_RATE% %TEST_FILE% -v

echo.
echo ========================================
echo Test completed!
echo Check received_file.dat for results
echo ========================================

pause
