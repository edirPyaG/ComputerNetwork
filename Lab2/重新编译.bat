@echo off
REM 修复参数验证后重新编译

echo ╔════════════════════════════════════════════════╗
echo ║   修复参数验证Bug - 重新编译                  ║
echo ╚════════════════════════════════════════════════╝
echo.

REM 检查nmake
where nmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    color 0C
    echo [错误] 未找到 nmake
    echo.
    echo 请在 Developer Command Prompt 中运行此脚本
    pause
    exit /b 1
)

echo 修复内容:
echo   ✓ 参数验证 - 防止窗口大小为0
echo   ✓ 错误提示 - 参数格式错误时显示帮助
echo   ✓ 范围检查 - 窗口大小必须在1-256之间
echo.

echo [1/2] 清理旧的编译产物...
nmake clean

echo.
echo [2/2] 重新编译...
nmake

if %ERRORLEVEL% EQU 0 (
    color 0A
    echo.
    echo ╔════════════════════════════════════════════════╗
    echo ║   编译成功！                                   ║
    echo ╚════════════════════════════════════════════════╝
    echo.
    echo 现在可以正确使用了！
    echo.
    echo 快速测试:
    echo   scripts\test_router_localhost.bat
    echo.
    echo 传输图片:
    echo   scripts\test_image.bat
    echo.
) else (
    color 0C
    echo.
    echo 编译失败！
)

pause
