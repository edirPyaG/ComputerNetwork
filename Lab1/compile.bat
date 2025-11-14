@echo off
REM 编译脚本 - 请在 Visual Studio Developer Command Prompt 中运行

echo ======================================
echo  编译 Lab1 聊天系统
echo ======================================
echo.

REM 确保在正确的目录
cd /d "%~dp0"

echo [1/7] 创建必要的目录...
if not exist "build" mkdir "build"
if not exist "data" mkdir "data"

echo.
echo [2/7] 编译 Common.obj...
cl.exe /EHsc /std:c++17 /utf-8 /I include /W4 /c src\Common.cpp /Fobuild\Common.obj
if %ERRORLEVEL% NEQ 0 (
    echo 编译 Common.cpp 失败！
    pause
    exit /b 1
)

echo.
echo [3/7] 编译 Storage.obj...
cl.exe /EHsc /std:c++17 /utf-8 /I include /W4 /c src\Storage.cpp /Fobuild\Storage.obj
if %ERRORLEVEL% NEQ 0 (
    echo 编译 Storage.cpp 失败！
    pause
    exit /b 1
)

echo.
echo [4/7] 编译 sqlite3.obj (可能需要几秒钟)...
cl.exe /EHsc /std:c++17 /utf-8 /I include /W4 /c lib\sqlitex64\sqlite3.c /Fobuild\sqlite3.obj
if %ERRORLEVEL% NEQ 0 (
    echo 编译 sqlite3.c 失败！
    pause
    exit /b 1
)

echo.
echo [5/7] 编译 Client.obj...
cl.exe /EHsc /std:c++17 /utf-8 /I include /W4 /c src\Client.cpp /Fobuild\Client.obj
if %ERRORLEVEL% NEQ 0 (
    echo 编译 Client.cpp 失败！
    pause
    exit /b 1
)

echo.
echo [6/7] 链接 Client.exe...
cl.exe build\Client.obj build\Common.obj build\Storage.obj build\sqlite3.obj /link /OUT:build\Client.exe ws2_32.lib
if %ERRORLEVEL% NEQ 0 (
    echo 链接 Client.exe 失败！
    pause
    exit /b 1
)

echo.
echo [7/7] 编译 Server.exe...
cl.exe /EHsc /std:c++17 /utf-8 /I include /W4 /c src\Server.cpp /Fobuild\Server.obj
if %ERRORLEVEL% NEQ 0 (
    echo 编译 Server.cpp 失败！
    pause
    exit /b 1
)

cl.exe build\Server.obj build\Common.obj build\sqlite3.obj /link /OUT:build\Server.exe ws2_32.lib
if %ERRORLEVEL% NEQ 0 (
    echo 链接 Server.exe 失败！
    pause
    exit /b 1
)

echo.
echo ======================================
echo  ✅ 编译成功！
echo ======================================
echo.
echo 生成的文件：
echo   - build\Client.exe
echo   - build\Server.exe
echo.
echo 使用方法：
echo   1. 先运行：build\Server.exe
echo   2. 再运行：build\Client.exe（可以多个）
echo.
pause
