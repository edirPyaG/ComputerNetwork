@echo off
REM RDT Protocol - Router Mode Test (Localhost/单机测试)
REM 在本机上通过不同端口模拟三个节点

setlocal EnableDelayedExpansion

echo ========================================
echo RDT - Router 单机测试模式
echo ========================================
echo.

REM ===== 单机测试配置（使用 localhost）=====

REM Sender 配置
set SENDER_IP=127.0.0.1
set SENDER_PORT=12000

REM Router 配置
set ROUTER_IP=127.0.0.1
set ROUTER_PORT=12001

REM Receiver 配置
set RECEIVER_IP=127.0.0.1
set RECEIVER_PORT=12002

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

echo ========================================
echo 单机测试拓扑 (localhost)
echo ========================================
echo.
echo   Sender (你的程序)
echo   127.0.0.1:12000
echo         ^|
echo         v [发送数据到 Router]
echo   Router (需手动启动)
echo   127.0.0.1:12001
echo         ^|
echo         v [转发到 Receiver + 丢包/延时]
echo   Receiver (你的程序)
echo   127.0.0.1:12002
echo.
echo ========================================
echo RDT 参数
echo ========================================
echo 发送窗口: %SEND_WIN% packets
echo 接收窗口: %RECV_WIN% packets
echo 文件: %TEST_FILE%
echo 发送端丢包率: %LOSS_RATE% (由Router控制)
echo ========================================
echo.
echo.

color 0E
echo ┌─────────────────────────────────────────┐
echo │  步骤 1: 启动 Router 程序并配置         │
echo │  (详见 docs\速查卡.txt)                 │
echo └─────────────────────────────────────────┘
echo.
echo 请先手动启动 Router.exe，然后配置:
echo.
echo   路由器IP:    %ROUTER_IP%
echo   端口:        %ROUTER_PORT%
echo   服务器IP:    %RECEIVER_IP%
echo   服务器端口:  %RECEIVER_PORT%
echo   丢包率:      5.0  (百分比，建议5-10)
echo   延时:        10   (毫秒，建议10-50)
echo.
echo 配置完成后点击「确定」按钮
echo 看到 "Router Ready!" 后继续
echo.
pause
echo.

color 0A
echo ┌─────────────────────────────────────────┐
echo │  步骤 2: 启动 Receiver                  │
echo └─────────────────────────────────────────┘
echo.
start "RDT Receiver [localhost:%RECEIVER_PORT%]" cmd /k "%RECEIVER% %RECV_WIN% %RECEIVER_IP% %RECEIVER_PORT%"
echo ✓ Receiver 已在新窗口启动
echo   监听地址: %RECEIVER_IP%:%RECEIVER_PORT%
echo.
timeout /t 2 /nobreak >nul

color 0B
echo ┌─────────────────────────────────────────┐
echo │  步骤 3: 启动 Sender                    │
echo └─────────────────────────────────────────┘
echo.
echo Sender 参数:
echo   本地地址: %SENDER_IP%:%SENDER_PORT%
echo   目标地址: %ROUTER_IP%:%ROUTER_PORT% (Router)
echo   窗口大小: %SEND_WIN%
echo   测试文件: %TEST_FILE%
echo.

REM 启动 Sender
"%SENDER%" %SEND_WIN% %LOSS_RATE% %TEST_FILE% %SENDER_IP% %SENDER_PORT% %ROUTER_IP% %ROUTER_PORT%

echo.
color 0A
echo ========================================
echo 测试完成!
echo ========================================
echo.
echo 查看 received_file.dat 验证接收结果
echo.
echo 在 Router 窗口中可以看到丢包/延时日志
echo.

pause
