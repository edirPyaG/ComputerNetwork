# Lab2 快速使用指南

## 项目已完成重构！

项目已从 CMake 迁移到 Makefile，并完成结构化整理。

### 当前项目结构

```
Lab2/
├── src/              # 源代码
├── include/          # 头文件
├── bin/              # 可执行文件 (按架构)
├── obj/              # 目标文件 (按架构)
├── scripts/          # 构建和测试脚本
├── docs/             # 文档
├── Makefile          # 构建配置
└── README.md         # 详细说明
```

## 快速开始

### 1. 编译程序

**重要**: 必须在 Visual Studio Developer Command Prompt 中运行！

```cmd
REM 方法1: 使用构建脚本（推荐）
scripts\build.bat

REM 方法2: 直接使用 nmake
nmake
```

### 2. 运行测试

```cmd
REM 使用测试脚本（自动启动收发端）
scripts\test.bat 32 0.05 test_file.dat
```

或手动运行：

```cmd
REM 终端1: 启动接收端
bin\ARM64\receiver.exe 64

REM 终端2: 启动发送端
bin\ARM64\sender.exe 32 0.05 test_file.dat -v
```

### 3. 清理编译

```cmd
nmake clean
```

## 关键特性

✅ **自动架构检测**: ARM64 或 x64 自动识别
✅ **SACK 支持**: 选择确认提高性能
✅ **TCP RENO**: 完整拥塞控制实现
✅ **流量控制**: 滑动窗口机制
✅ **性能统计**: 详细吞吐量和丢包分析

更多详细信息请查看 `README.md`
