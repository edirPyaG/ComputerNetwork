# Router 模式快速测试指南

## 🎯 最简单的测试方法（推荐）

### 方法一：单机测试（localhost）

1. **编译程序**（在 Developer Command Prompt 中）：
   ```cmd
   cd c:\学习\计算机网络\Lab2
   nmake rebuild
   ```

2. **启动 Router 程序**：
   ```
   路由器IP:    127.0.0.1
   端口:        12001
   服务器IP:    127.0.0.1
   服务器端口:  12002
   丢包率:      5.0
   延时:        10
   ```
   点击「确定」，等待显示 `Router Ready!`

3. **运行测试脚本**：
   ```cmd
   scripts\test_router_localhost.bat
   ```

### 方法二：手动测试

**终端1 - Receiver**:
```cmd
bin\x64\receiver.exe 64 127.0.0.1 12002
```

**终端2 - Sender**:
```cmd
bin\x64\sender.exe 32 0.0 test_file.dat 127.0.0.1 12000 127.0.0.1 12001
```

## 📝 参数说明

### Sender 命令格式
```
sender.exe [窗口大小] [丢包率] [文件名] [本地IP] [本地端口] [目标IP] [目标端口]
```

**示例**:
```cmd
sender.exe 32 0.0 test_file.dat 127.0.0.1 12000 127.0.0.1 12001
           │  │   │             │         │     │         │
           │  │   │             │         │     │         └─ Router端口
           │  │   │             │         │     └─────────── Router IP
           │  │   │             │         └─────────────── Sender本地端口  
           │  │   │             └───────────────────────── Sender本地IP
           │  │   └─────────────────────────────────────── 传输文件
           │  └─────────────────────────────────────────── 丢包率(设0,由Router控制)
           └────────────────────────────────────────────── 发送窗口

### Receiver 命令格式
```
receiver.exe [窗口大小] [本地IP] [本地端口]
```

**示例**:
```cmd
receiver.exe 64 127.0.0.1 12002
             │  │         │
             │  │         └─ 监听端口
             │  └─────────── 绑定IP
             └────────────── 接收窗口
```

## 🧪 Router 配置建议

### 测试场景1: 基线测试
```
丢包率: 0%
延时:   0ms
```
**目的**: 验证协议基本功能，无丢包无延时

### 测试场景2: 轻度丢包
```
丢包率: 5%
延时:   10ms
```
**目的**: 测试快速重传和拥塞控制

### 测试场景3: 中度丢包
```
丢包率: 10%
延时:   50ms
```
**目的**: 验证协议在较差网络下的性能

### 测试场景4: 高丢包
```
丢包率: 20%
延时:   100ms
```
**目的**: 压力测试，验证协议稳定性

## ✅ 成功测试的标志

1. **连接建立**:
   ```
   [Sender] Starting handshake...
   [Sender] Handshake complete.
   [Receiver] SYN received. Initializing connection...
   ```

2. **数据传输**:
   - Sender 显示进度
   - Router 窗口显示转发/丢包/延时日志
   - Receiver 显示接收统计

3. **连接关闭**:
   ```
   [Sender] Sending FIN...
   [Receiver] FIN received. Closing connection...
   ```

4. **统计信息**:
   ```
   Total Bytes Received: 71680 bytes
   Retransmitted Packets: 5
   Transmission Time: 2.5 seconds
   ```

## 🔍 日志分析

### Router 日志示例
```
Router Ready!
Misscount: 20        ← 每20个包丢1个(5%丢包率)
Delay: 10 ms         ← 延时10毫秒
[丢包] seq=1024      ← 丢弃了序号1024的包
[延时] seq=2048      ← 延时了序号2048的包
```

### 预期性能（70KB文件）

| 丢包率 | 延时 | 传输时间 | 重传次数 | 吞吐量 |
|-------|------|---------|---------|--------|
| 0% | 0ms | 1-2秒 | 0 | 0.3-0.5 Mbps |
| 5% | 10ms | 2-3秒 | 3-5 | 0.2-0.3 Mbps |
| 10% | 50ms | 3-5秒 | 7-10 | 0.1-0.2 Mbps |
| 20% | 100ms | 6-10秒 | 14-20 | 0.05-0.1 Mbps |

## ⚠️ 常见问题

**Q: Router 显示 "Bind failed"**  
A: 端口被占用，换一个端口号

**Q: Sender 一直发 SYN 无响应**  
A: 检查 Sender 的目标地址是否正确指向 Router

**Q: Receiver 没有收到数据**  
A: 检查 Router 的转发目标是否正确指向 Receiver

**Q: 传输速度很慢**  
A: 降低 Router 丢包率或减小延时
