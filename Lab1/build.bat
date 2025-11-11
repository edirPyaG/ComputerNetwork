@echo off
echo Compiling Chat Application...

REM 创建 build 目录
if not exist build mkdir build

REM 编译 Common.cpp
cl.exe /EHsc /std:c++17 /utf-8 /I include /c src\Common.cpp /Fo:build\Common.obj
if %errorlevel% neq 0 (
    echo Failed to compile Common.cpp
    exit /b 1
)

REM 编译 Server.cpp
cl.exe /EHsc /std:c++17 /utf-8 /I include /c src\Server.cpp /Fo:build\Server.obj
if %errorlevel% neq 0 (
    echo Failed to compile Server.cpp
    exit /b 1
)

REM 编译 Client.cpp
cl.exe /EHsc /std:c++17 /utf-8 /I include /c src\Client.cpp /Fo:build\Client.obj
if %errorlevel% neq 0 (
    echo Failed to compile Client.cpp
    exit /b 1
)

REM 链接 Server
cl.exe build\Server.obj build\Common.obj /Fe:build\Server.exe /link ws2_32.lib
if %errorlevel% neq 0 (
    echo Failed to link Server.exe
    exit /b 1
)

REM 链接 Client
cl.exe build\Client.obj build\Common.obj /Fe:build\Client.exe /link ws2_32.lib
if %errorlevel% neq 0 (
    echo Failed to link Client.exe
    exit /b 1
)

echo Build completed successfully!
echo Server: build\Server.exe
echo Client: build\Client.exe
