@echo off
REM RDT Protocol - Router Mode Test Script
REM 通过 Router 程序转发数据包进行测试

setlocal EnableDelayedExpansion

echo ========================================
echo RDT Protocol - Router Mode Test
echo ========================================
echo.

REM ===== 配置参数 =====
REM 根据 Router 教程配置拓扑

REM Sender (Client) 配置
set SENDER_IP=192.168.10.1
set SENDER_PORT=12

REM Router 配置
set ROUTER_IP=192.168.10.2
set ROUTER_PORT=34

REM Receiver (Server) 配置
set RECEIVER_IP=192.168.10.3
set RECEIVER_PORT=56

REM RDT 协议参数
set SEND_WIN=32
set RECV_WIN=64
set LOSS_RATE=0.0
set TEST_FILE=test_file.dat

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

REM 检查可执行文件
if not exist "%SENDER%" (
    echo ERROR: sender.exe not found in %BIN_DIR%
    echo Please run: nmake
    pause
    exit /b 1
)

if not exist "%RECEIVER%" (
    echo ERROR: receiver.exe not found in %BIN_DIR%
    echo Please run: nmake
    pause
    exit /b 1
)

echo ========================================
echo 网络拓扑配置
echo ========================================
echo Sender:   %SENDER_IP%:%SENDER_PORT%
echo   ^|
echo   v (发送数据)
echo Router:   %ROUTER_IP%:%ROUTER_PORT%
echo   ^|
echo   v (转发+丢包/延时)
echo Receiver: %RECEIVER_IP%:%RECEIVER_PORT%
echo ========================================
echo.
echo RDT 参数:
echo   - 发送窗口: %SEND_WIN% packets
echo   - 接收窗口: %RECV_WIN% packets
echo   - 丢包率: %LOSS_RATE% (由 Router 控制)
echo   - 测试文件: %TEST_FILE%
echo ========================================
echo.

echo 重要提示 (详见 docs\ROUTER_CONFIG.txt):
echo 1. 请先启动 Router 程序并配置:
echo    - 路由器 IP: %ROUTER_IP%
echo    - 端口: %ROUTER_PORT%
echo    - 服务器 IP: %RECEIVER_IP%
echo    - 服务器端口: %RECEIVER_PORT%
echo    - 丢包率: (在 Router 中设置)
echo    - 延时: (在 Router 中设置)
echo.
echo 2. 按任意键继续将启动 Receiver 和 Sender
echo.
pause

REM 启动 Receiver
echo.
echo [步骤 1/2] 启动 Receiver...
start "RDT Receiver [Router Mode]" cmd /k "%RECEIVER% %RECV_WIN% %RECEIVER_IP% %RECEIVER_PORT%"

REM 等待 Receiver 启动
timeout /t 2 /nobreak >nul

REM 启动 Sender
echo [步骤 2/2] 启动 Sender...
echo.
echo Sender 命令行:
echo %SENDER% %SEND_WIN% %LOSS_RATE% %TEST_FILE% %SENDER_IP% %SENDER_PORT% %ROUTER_IP% %ROUTER_PORT%
echo.

REM 参数说明:
REM sender.exe [发送窗口] [丢包率] [文件] [本地IP] [本地端口] [目标IP] [目标端口]
"%SENDER%" %SEND_WIN% %LOSS_RATE% %TEST_FILE% %SENDER_IP% %SENDER_PORT% %ROUTER_IP% %ROUTER_PORT%

echo.
echo ========================================
echo 测试完成!
echo ========================================
echo 检查 received_file.dat 查看接收结果
echo.

pause
