# 实验3：配置Web服务器，捕获HTTP报文并分析

**专业**：信息安全  
**学号**：2312130  
**姓名**：景千夏 (Qianxia Jing)

---

## 一、实验目的
1. 掌握Web服务器的配置方法。
2. 理解HTTP协议的工作过程。
3. 掌握使用Wireshark分析网络协议报文的方法。

## 二、实验环境
- **操作系统**：Windows
- **Web服务器**：Python `http.server` (监听端口 8080)
- **浏览器**：Chrome / Edge / Firefox
- **抓包工具**：Wireshark

## 三、实验过程及结果

### 1. Web服务器搭建与页面访问
*(此处截图证明服务器已运行，浏览器能成功访问 localhost:8080 并看到 6 张图片)*

**操作步骤**：
1. 运行 `run_server.py` 启动服务器。
2. 浏览器访问 `http://localhost:8080`。

### 2. Wireshark 捕获 HTTP 报文
*(此处截图显示 Wireshark 过滤 `http` 后的抓包列表)*

**操作步骤**：
1. 打开 Wireshark，选择 Wlan 或 Loopback 适配器（如果是本地访问，通常需要选择 **Adapter for loopback traffic capture**）。
2. 开始捕获。
3. 刷新浏览器页面。
4. 停止捕获，使用过滤器 `http` 筛选报文。

## 四、报文封装层次分析

选取一个 HTTP GET 请求报文（例如请求 `index.html`）进行详细分析。

### (1) 应用层 (HTTP)
- **协议**：Hypertext Transfer Protocol
- **功能**：定义浏览器与服务器之间的请求与响应格式。
- **关键字段**：
    - `Request Method`: GET
    - `Request URI`: /index.html
    - `User-Agent`: (浏览器信息)

### (2) 传输层 (TCP)
- **协议**：Transmission Control Protocol
- **功能**：提供可靠的字节流传输。
- **关键字段**：
    - **Source Port**: (随机高端口，例如 54321)
    - **Destination Port**: 8080 (服务器端口)
    - **Sequence Number**: (序列号)
    - **Flags**: PSH, ACK (表示有数据且确认)

### (3) 互连层 (Network Layer / IP)
- **协议**：Internet Protocol Version 4/6 (IPv4/IPv6)
- **功能**：负责逻辑寻址和路由。
- **关键字段**：
    - **Source IP**: 127.0.0.1 (Localhost)
    - **Destination IP**: 127.0.0.1 (Localhost)
    - **Protocol**: TCP (6)

### (4) 数据链路层 (Data Link Layer)
- **协议**：Ethernet II (或者 Null/Loopback，取决于抓包接口)
- **功能**：负责物理寻址（MAC地址）和帧传输。
- **关键字段**：
    - **Type**: IPv4 (0x0800)
    - *注：在 Loopback 接口抓包时，链路层头可能与标准以太网不同。*

---

## 五、HTTP 交互过程分析

### 1. TCP 三次握手 (Connection Establishment)
在 HTTP 请求发送前，首先建立了 TCP 连接：
1. **SYN**: 客户端 -> 服务器 (试图连接 8080 端口)
2. **SYN, ACK**: 服务器 -> 客户端 (接受连接)
3. **ACK**: 客户端 -> 服务器 (连接建立)

### 2. HTTP 请求与响应
#### 请求 (Request)
- 客户端发送 `GET /index.html HTTP/1.1`。
- 包含 `Host: localhost:8080` 等头部信息。

#### 响应 (Response)
- 服务器回复 `HTTP/1.0 200 OK` (或其他版本)。
- 包含 `Content-Type: text/html`。
- **Data**: HTML 文件的实际内容。

### 3. 后续资源请求
- 浏览器解析 HTML 后，发现 `<img>` 标签。
- 浏览器通过**新建 TCP 连接**或**复用现有连接** (Keep-Alive) 发送对 `image1.svg` 至 `image6.svg` 的 GET 请求。
- 服务器依次返回 200 OK 及图片数据。

### 4. TCP 连接释放 (Four-Way Wave)
页面加载完成后，连接可能保持一段时间 (Keep-Alive) 或立即断开：
1. **FIN, ACK**: 主动关闭方发送断开请求。
2. **ACK**: 被动关闭方确认。
3. **FIN, ACK**: 被动方也发送断开请求。
4. **ACK**: 主动方确认，连接关闭。

---

## 六、实验总结
通过本次实验，成功搭建了 Python Web 服务器，并使用 Wireshark 观察到了 HTTP 协议在 TCP/IP 协议栈之上的工作流程。验证了 HTTP 的请求-响应模式以及底层的封装结构。
