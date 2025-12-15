@echo off
REM RDT Protocol Build Script
REM Auto-detect architecture and compile

echo ========================================
echo RDT Protocol Build Script
echo ========================================

REM Check if nmake is available
where nmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: nmake not found!
    echo Please run this from Visual Studio Developer Command Prompt
    echo Or run: "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    pause
    exit /b 1
)

REM Build using Makefile
nmake

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build completed successfully!
    echo ========================================
    echo.
    echo Executables are in bin\ directory
    echo.
) else (
    echo.
    echo Build failed with error code %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

pause
