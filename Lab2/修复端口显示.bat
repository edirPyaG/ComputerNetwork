@echo off
echo 修复端口显示bug，重新编译...
echo.

where nmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [错误] 请在 Developer Command Prompt 中运行
    pause
    exit /b 1
)

nmake rebuild

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✓ 编译成功！
    echo.
    echo 现在测试:
    echo   bin\ARM64\receiver.exe 64 127.0.0.1 12002
    echo.
    echo 应该显示: Receiver listening on 127.0.0.1:12002...
    echo.
)

pause
