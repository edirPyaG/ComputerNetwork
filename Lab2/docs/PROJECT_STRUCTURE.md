# Lab2 项目结构说明

## 📁 完整目录结构

```
Lab2/
├── src/                          # 源代码目录
│   ├── sender.cpp               # 发送端实现（支持Router模式）
│   ├── receiver.cpp             # 接收端实现（支持Router模式）
│   └── config.cpp               # 全局配置变量
│
├── include/                      # 头文件目录
│   └── rdt_protocol.h           # RDT协议头文件定义
│
├── bin/                          # 可执行文件目录（按架构分类）
│   ├── ARM64/                   # ARM64架构编译输出
│   │   ├── sender.exe
│   │   └── receiver.exe
│   └── x64/                     # x64架构编译输出
│       ├── sender.exe
│       └── receiver.exe
│
├── obj/                          # 目标文件目录（按架构分类）
│   ├── ARM64/                   # ARM64目标文件
│   │   ├── sender.obj
│   │   ├── receiver.obj
│   │   └── config.obj
│   └── x64/                     # x64目标文件
│       ├── sender.obj
│       ├── receiver.obj
│       └── config.obj
│
├── scripts/                      # 脚本目录
│   ├── build.bat                # 编译脚本
│   ├── test.bat                 # 普通测试脚本
│   ├── test_router.bat          # Router多机测试脚本
│   ├── test_router_localhost.bat # Router单机测试脚本（推荐）
│   ├── rebuild.bat              # 重新编译脚本
│   └── 一键测试.bat             # 一键编译+测试脚本
│
├── docs/                         # 文档目录
│   ├── ROUTER_使用说明.md       # Router模式完整使用指南
│   ├── ROUTER_QUICKSTART.md     # Router快速开始指南
│   ├── ROUTER_TEST_GUIDE.md     # Router详细测试指南
│   ├── ROUTER_CONFIG.txt        # Router配置速查表
│   ├── 速查卡.txt               # 快速参考卡片
│   └── QUICKSTART.md            # 项目快速开始指南
│
├── lab2测试环境/                 # 实验测试环境（Router程序等）
│   └── router/
│       └── Router教程.md
│
├── Makefile                      # 构建配置文件（支持架构自动检测）
├── README.md                     # 项目说明文档
├── 整理结构.bat                  # 项目结构整理脚本
│
└── 测试文件/
    ├── test_file.dat            # 测试数据文件
    └── received_file.dat        # 接收到的文件
```

## 🎯 文件功能说明

### 源代码 (src/)
- **sender.cpp**: 多线程发送端，实现TCP RENO拥塞控制、SACK、流量控制
- **receiver.cpp**: 单线程接收端，实现SACK选择确认、乱序包缓存
- **config.cpp**: 全局配置变量（窗口大小、丢包率等）

### 头文件 (include/)
- **rdt_protocol.h**: 协议定义、数据结构、校验和函数、日志工具

### 脚本 (scripts/)
| 脚本文件 | 用途 | 使用场景 |
|---------|------|---------|
| `build.bat` | 基本编译 | 首次编译或修改代码后 |
| `rebuild.bat` | 清理并重编译 | 修复ACK问题后必须运行 |
| `一键测试.bat` | 编译+Router测试 | **推荐：快速测试全流程** |
| `test.bat` | 直连测试 | 本地测试（无Router） |
| `test_router_localhost.bat` | Router单机测试 | **推荐：Router模式测试** |
| `test_router.bat` | Router多机测试 | 多台电脑/虚拟机测试 |

### 文档 (docs/)
| 文档文件 | 内容 | 推荐阅读顺序 |
|---------|------|-------------|
| `速查卡.txt` | 最精简参考 | ⭐ 第一个看（打印随身携带） |
| `ROUTER_QUICKSTART.md` | 快速开始 | ⭐ 快速上手 |
| `ROUTER_使用说明.md` | 完整指南 | ⭐⭐ 全面了解 |
| `ROUTER_CONFIG.txt` | 配置表格 | ⭐ 配置Router时查看 |
| `ROUTER_TEST_GUIDE.md` | 详细测试 | ⭐⭐⭐ 写实验报告参考 |
| `QUICKSTART.md` | 项目入门 | 首次使用项目 |

## 🚀 快速使用流程

### 1. 首次使用
```cmd
# 1. 打开 Developer Command Prompt
# 2. 进入项目目录
cd c:\学习\计算机网络\Lab2

# 3. 整理项目结构（首次运行）
整理结构.bat

# 4. 编译项目
nmake

# 5. 查看快速指南
type docs\速查卡.txt
```

### 2. Router模式测试（推荐）
```cmd
# 方案A: 一键测试（最简单）
scripts\一键测试.bat

# 方案B: 手动步骤测试
# 步骤1: 配置并启动Router（参考 docs\ROUTER_CONFIG.txt）
# 步骤2: 运行测试脚本
scripts\test_router_localhost.bat
```

### 3. 直连模式测试
```cmd
scripts\test.bat
```

## 📊 编译输出说明

### 架构自动检测
Makefile会自动检测当前系统架构：
- **ARM64设备** → 编译到 `bin\ARM64\` 和 `obj\ARM64\`
- **x64设备** → 编译到 `bin\x64\` 和 `obj\x64\`

### 编译命令
```cmd
nmake              # 编译当前架构
nmake clean        # 清理所有编译产物
nmake clean-arch   # 清理当前架构编译产物
nmake rebuild      # 重新编译
nmake help         # 显示帮助
```

## 📝 重要注意事项

### ✅ 正确的文件位置
- ✅ 所有 `.cpp` 文件 → `src/`
- ✅ 所有 `.h` 文件 → `include/`
- ✅ 所有 `.bat` 脚本 → `scripts/`
- ✅ 所有 `.md` 和 `.txt` 文档 → `docs/`
- ✅ 编译输出 `.exe` → `bin/[架构]/`
- ✅ 编译中间文件 `.obj` → `obj/[架构]/`

### ❌ 不要直接放在根目录
- ❌ 不要在根目录放源代码
- ❌ 不要在根目录放脚本
- ❌ 不要在根目录放文档（README.md除外）

### 🔄 如果文件位置错误
运行整理脚本：
```cmd
整理结构.bat
```

## 🎓 实验报告建议结构

使用 `docs/ROUTER_TEST_GUIDE.md` 中的测试数据表格：
1. 测试环境说明（网络拓扑图）
2. 不同丢包率下的性能对比
3. 不同窗口大小的影响分析
4. TCP RENO算法实现说明
5. 性能图表（吞吐量、重传次数等）

## 📞 获取帮助

- 快速参考：`docs\速查卡.txt`
- 命令帮助：`nmake help`
- 完整文档：`docs\ROUTER_使用说明.md`
- Router教程：`lab2测试环境\router\Router教程.md`

---

**推荐工作流程**：
1. 运行 `整理结构.bat` 确保文件位置正确
2. 阅读 `docs\速查卡.txt` 快速了解
3. 运行 `scripts\一键测试.bat` 开始测试
4. 参考 `docs\ROUTER_使用说明.md` 进行详细测试
