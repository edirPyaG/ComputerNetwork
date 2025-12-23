@echo off
REM Router 模式传输图片文件测试

setlocal EnableDelayedExpansion

echo ╔════════════════════════════════════════════════╗
echo ║   RDT - 图片文件传输测试 (Router模式)          ║
echo ╚════════════════════════════════════════════════╝
echo.

REM 检测架构
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

REM 检查测试图片
set TEST_IMAGE=lab2测试环境\测试文件\2.jpg
if not exist "%TEST_IMAGE%" (
    echo [错误] 测试图片不存在: %TEST_IMAGE%
    echo.
    echo 请确保测试文件在正确位置
    pause
    exit /b 1
)

REM 获取文件大小
for %%A in ("%TEST_IMAGE%") do set FILE_SIZE=%%~zA

echo ════════════════════════════════════════════════
echo 测试文件信息
echo ════════════════════════════════════════════════
echo 文件: %TEST_IMAGE%
echo 大小: %FILE_SIZE% 字节
echo 架构: %ARCH%
echo ════════════════════════════════════════════════
echo.

REM Router 配置
set SENDER_IP=127.0.0.1
set SENDER_PORT=12000
set ROUTER_IP=127.0.0.1
set ROUTER_PORT=12001
set RECEIVER_IP=127.0.0.1
set RECEIVER_PORT=12002

REM RDT 参数
set SEND_WIN=32
set RECV_WIN=64
set LOSS_RATE=0.0

echo ┌─────────────────────────────────────────┐
echo │  网络拓扑                               │
echo └─────────────────────────────────────────┘
echo.
echo   Sender          Router          Receiver
echo   %SENDER_IP%:%-5s → %ROUTER_IP%:%-5s → %RECEIVER_IP%:%RECEIVER_PORT%
echo.
echo ════════════════════════════════════════════════
echo 步骤 1: 配置 Router
echo ════════════════════════════════════════════════
echo.
echo 请启动 Router.exe 并配置:
echo.
echo   路由器IP:      %ROUTER_IP%
echo   端口:          %ROUTER_PORT%
echo   服务器IP:      %RECEIVER_IP%
echo   服务器端口:    %RECEIVER_PORT%
echo   丢包率:        5.0
echo   延时:          10
echo.
echo 配置完成后点击「确定」，看到 "Router Ready!" 后按任意键继续
pause
echo.

echo ════════════════════════════════════════════════
echo 步骤 2: 启动 Receiver
echo ════════════════════════════════════════════════
start "RDT Receiver [%RECEIVER_IP%:%RECEIVER_PORT%]" cmd /k "%RECEIVER% %RECV_WIN% %RECEIVER_IP% %RECEIVER_PORT%"
echo ✓ Receiver 已启动
timeout /t 2 /nobreak >nul
echo.

echo ════════════════════════════════════════════════
echo 步骤 3: 启动 Sender 传输图片
echo ════════════════════════════════════════════════
echo.
echo 完整命令:
echo %SENDER% %SEND_WIN% %LOSS_RATE% "%TEST_IMAGE%" %SENDER_IP% %SENDER_PORT% %ROUTER_IP% %ROUTER_PORT%
echo.
echo 开始传输...
echo.

REM 启动 Sender 传输图片
"%SENDER%" %SEND_WIN% %LOSS_RATE% "%TEST_IMAGE%" %SENDER_IP% %SENDER_PORT% %ROUTER_IP% %ROUTER_PORT%

echo.
echo ════════════════════════════════════════════════
echo 传输完成！
echo ════════════════════════════════════════════════
echo.
echo 验证接收的图片:
echo   原始文件: %TEST_IMAGE%
echo   接收文件: received_file.dat
echo.
echo 文件对比:
fc /b "%TEST_IMAGE%" received_file.dat
echo.

if %ERRORLEVEL% EQU 0 (
    echo ✓ 文件完全一致！传输成功！
    echo.
    echo 你可以将 received_file.dat 重命名为 received.jpg 查看
) else (
    echo ✗ 文件不一致，传输可能有错误
)

echo.
pause
