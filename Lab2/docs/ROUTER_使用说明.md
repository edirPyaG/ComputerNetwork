# 🚀 Router 模式适配完成 - 使用说明

## ✅ 已完成的修改

### 1. 代码修改
- ✅ **Sender** 支持配置本地 IP、本地端口、目标 IP、目标端口
- ✅ **Receiver** 支持配置本地 IP 和端口
- ✅ **修复 ACK 丢包问题**（控制包不丢，只丢数据包）

### 2. 新增文件
- ✅ `scripts/test_router_localhost.bat` - 单机测试脚本（推荐）
- ✅ `scripts/test_router.bat` - 多机测试脚本
- ✅ `ROUTER_CONFIG.txt` - 配置速查表
- ✅ `ROUTER_QUICKSTART.md` - 快速开始指南
- ✅ `ROUTER_TEST_GUIDE.md` - 详细测试指南

## 📋 立即开始测试（3步）

### 第1步：编译程序
在 **Developer Command Prompt** 中运行：
```cmd
cd c:\学习\计算机网络\Lab2
nmake rebuild
```

### 第2步：配置 Router
启动 `Router.exe`，填入以下配置：
```
路由器IP:      127.0.0.1
端口:          12001
服务器IP:      127.0.0.1
服务器端口:    12002
丢包率:        5.0
延时:          10
```
点击 **「确定」** 按钮，等待显示 `Router Ready!`

### 第3步：运行测试
```cmd
scripts\test_router_localhost.bat
```

## 📝 具体测试参数

### 方案A：自动化测试（推荐新手）

**运行脚本**：
```cmd
scripts\test_router_localhost.bat
```

脚本会自动：
1. 提示你配置 Router
2. 启动 Receiver（新窗口）
3. 启动 Sender 并传输数据

### 方案B：手动测试（便于调试）

**终端1 - 启动 Receiver**：
```cmd
bin\x64\receiver.exe 64 127.0.0.1 12002
```

**终端2 - 启动 Sender**：
```cmd
bin\x64\sender.exe 32 0.0 test_file.dat 127.0.0.1 12000 127.0.0.1 12001
```

### 参数说明

#### Sender 完整参数
```
bin\x64\sender.exe [窗口] [丢包率] [文件] [本地IP] [本地端口] [目标IP] [目标端口]
                   │     │       │      │       │         │       │
                   32    0.0     test   127...  12000     127...  12001
                   │     │       │      │       │         │       │
                   │     │       │      │       │         │       └─ Router端口
                   │     │       │      │       │         └──────── Router IP
                   │     │       │      │       └─────────────────── 本地端口
                   │     │       │      └─────────────────────────── 本地IP
                   │     │       └────────────────────────────────── 文件名
                   │     └────────────────────────────────────────── 丢包率(必须为0!)
                   └──────────────────────────────────────────────── 发送窗口
```

**重要**：`丢包率` 参数必须设为 `0.0`，因为丢包由 Router 控制！

#### Receiver 完整参数
```
bin\x64\receiver.exe [窗口] [本地IP] [本地端口]
                     │     │       │
                     64    127...  12002
                     │     │       │
                     │     │       └─ 监听端口
                     │     └──────── 绑定IP
                     └────────────── 接收窗口
```

## 🧪 测试场景配置

### 场景1：基线测试（验证功能）
**Router配置**：
- 丢包率: `0`%
- 延时: `0` ms

**预期结果**：
- 传输时间: 1-2 秒
- 重传次数: 0
- 吞吐量: 0.3-0.5 Mbps

### 场景2：轻度丢包（常规测试）
**Router配置**：
- 丢包率: `5.0`%
- 延时: `10` ms

**预期结果**：
- 传输时间: 2-3 秒
- 重传次数: 3-5 次
- 观察快速重传机制

### 场景3：中度丢包（性能测试）
**Router配置**：
- 丢包率: `10.0`%
- 延时: `50` ms

**预期结果**：
- 传输时间: 3-5 秒
- 重传次数: 7-10 次
- 观察拥塞控制窗口变化

### 场景4：高压力测试
**Router配置**：
- 丢包率: `20.0`%
- 延时: `100` ms

**预期结果**：
- 传输时间: 6-10 秒
- 重传次数: 14-20 次
- 验证协议稳定性

### 场景5：窗口大小对比
保持 Router 配置不变（丢包率5%，延时10ms），修改 Sender 窗口：
```cmd
# 小窗口
sender.exe 16 0.0 test_file.dat 127.0.0.1 12000 127.0.0.1 12001

# 中窗口
sender.exe 32 0.0 test_file.dat 127.0.0.1 12000 127.0.0.1 12001

# 大窗口
sender.exe 64 0.0 test_file.dat 127.0.0.1 12000 127.0.0.1 12001
```

观察窗口大小对吞吐量的影响。

## 📊 测试数据记录表

| 测试场景 | 窗口 | 丢包率 | 延时 | 传输时间 | 重传次数 | 超时次数 | 吞吐量 |
|---------|------|--------|------|---------|---------|---------|--------|
| 基线 | 32 | 0% | 0ms | | | | |
| 场景2 | 32 | 5% | 10ms | | | | |
| 场景3 | 32 | 10% | 50ms | | | | |
| 场景4 | 32 | 20% | 100ms | | | | |
| 小窗口 | 16 | 5% | 10ms | | | | |
| 大窗口 | 64 | 5% | 10ms | | | | |

## ✅ 成功标志

### 1. Router 日志
```
Router Ready!
Misscount: 20        ← 丢包策略
Delay: 10 ms         ← 延时设置
[丢包] ...           ← 丢包事件
[延时] ...           ← 延时事件
```

### 2. Sender 输出
```
========== RDT Sender ==========
...
[Sender] Handshake complete.
[Sender] File size: 71680 bytes
...
[Sender] Waiting for all ACKs...
[Sender] Sending FIN...

========== Transmission Statistics ==========
Total Bytes Sent: 71680 bytes
Retransmitted Packets: 5
Timeouts: 2
Transmission Time: 2.5 seconds
Average Throughput: 0.23 Mbps
=============================================
```

### 3. Receiver 输出
```
========== RDT Receiver ==========
...
[Receiver] SYN received. Initializing connection...
[Receiver] FIN received. Closing connection...

========== Receiver Statistics ==========
Total Bytes Received: 71680 bytes
Total ACKs Sent: 75
Reception Time: 2.4 seconds
=========================================
```

### 4. 验证文件
检查生成的 `received_file.dat` 是否与 `test_file.dat` 内容一致：
```cmd
fc /b test_file.dat received_file.dat
```
应该显示：`FC: 未找到差异`

## ⚠️ 常见问题

### Q1: Router 显示 "Bind failed"
**原因**：端口被占用  
**解决**：更改端口号，例如改为 12011、12012

### Q2: Sender 一直发 SYN，无响应
**原因**：目标地址错误或 Router 未启动  
**解决**：
1. 检查 Sender 的目标 IP:端口 是否指向 Router
2. 确认 Router 显示 "Router Ready!"

### Q3: Receiver 没有收到数据
**原因**：Router 转发配置错误  
**解决**：检查 Router 的"服务器IP"和"服务器端口"是否正确

### Q4: 传输速度极慢（超过30秒）
**原因**：丢包率太高或之前的 ACK 丢包 bug  
**解决**：
1. 确保已重新编译（包含 ACK 修复）
2. 降低 Router 丢包率（建议 5-10%）

### Q5: 编译错误
**错误**：`nmake 不是内部或外部命令`  
**解决**：必须在 **Developer Command Prompt** 中运行

## 📁 相关文件

- `ROUTER_CONFIG.txt` - 配置速查表（可打印）
- `ROUTER_QUICKSTART.md` - 快速开始（简洁版）
- `ROUTER_TEST_GUIDE.md` - 详细测试指南（完整版）
- `scripts/test_router_localhost.bat` - 单机自动化测试
- `scripts/test_router.bat` - 多机测试脚本

## 🎓 实验报告建议

1. **记录不同丢包率下的性能数据**
2. **绘制图表**：
   - 丢包率 vs 吞吐量
   - 窗口大小 vs 吞吐量
   - 延时 vs 传输时间
3. **分析 TCP RENO 算法**：
   - 慢启动阶段的 cwnd 变化
   - 快速重传触发条件
   - 超时后的恢复过程
4. **截图**：
   - Router 日志窗口
   - Sender 统计信息
   - 网络拓扑图

---

**祝测试顺利！有问题随时问我 😊**
