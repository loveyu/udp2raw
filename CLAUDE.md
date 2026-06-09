# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

udp2raw 是一个 C++ 隧道工具，通过 raw socket 将 UDP 流量伪装为加密的 FakeTCP/UDP/ICMP 流量，用于绕过 UDP 防火墙限制和 QoS。采用 C++11 编写，无外部依赖（加密库和 libev 均内嵌）。

## 构建命令

```bash
# 本地构建（静态链接，O2 优化）
make all

# 调试构建
make debug      # 带 -D MY_DEBUG
make debug2     # 带 AddressSanitizer

# 快速构建（无优化，用于开发迭代）
make fast

# 清理
make clean

# CMake 仅用于生成 compile_commands.json（供 clangd/IDE 使用），不用于正式构建
mkdir -p build && cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

# Android 构建（NDK 搜索顺序：$ANDROID_NDK_HOME > $ANDROID_NDK > ~/Android/Sdk/ndk/*）
cd android && ./build_android.sh
```

交叉编译目标（需要对应工具链）：`make amd64`, `make arm`, `make mips24kc_be`, `make x86` 等。带 `_asm_aes` 后缀的目标使用硬件加速 AES（AES-NI / ARM NEON / MIPS 汇编）。

## 架构

### 客户端-服务端模型

客户端使用 **libev** 事件循环，服务端直接使用 **epoll**。两者有独立的状态机实现。

### 数据流路径

```
本地 UDP socket → 加密+认证+防重放 → raw socket（伪装为 TCP/UDP/ICMP）→ 网络 → 服务端 raw socket → 解密+验证 → 转发至目标
```

### 核心源文件及职责

| 文件 | 职责 |
|---|---|
| `main.cpp` | 入口，调用 `udp2raw_run()` |
| `client.cpp` | 客户端事件循环、状态机（idle→tcp_handshake→handshake1→handshake2→ready）、自动重连 |
| `server.cpp` | 服务端事件循环、多连接管理（epoll + conn_manager_t） |
| `network.cpp` | 最大文件（~3000行）。Raw socket 初始化、BPF 过滤器、三种模式的发包/收包、IP/TCP/UDP/ICMP 头构造与解析 |
| `connection.cpp` | 连接管理、防重放滑动窗口（4000包）、conv_manager（UDP 多路复用）、加解密收发 |
| `encrypt.cpp` | AES-128-CBC/CFB、XOR 加密；MD5/HMAC-SHA1/CRC32 认证；PBKDF2 密钥派生 |
| `common.cpp` | 参数解析、地址处理、校验和计算、`myexit()` |
| `misc.cpp` | 命令行参数处理、iptables 规则管理、全局状态 |

### 三种协议模式

- **FakeTCP**：模拟 TCP 三次握手、seq/ack，配合 iptables 阻止内核 TCP 栈干扰
- **UDP**：直通模式，UDP 封装
- **ICMP**：通过 ICMP 包隧道传输

### 关键设计

- **UDP 多路复用**：`conv_manager_t` 通过 conversation ID 在一条 raw 连接上复用多个 UDP 会话
- **防重放**：`anti_replay_t` 使用 4000 包的滑动窗口
- **连接恢复**：客户端用 `const_id` 在重连后恢复已有会话
- **BPF 过滤**：Linux 内核空间按端口过滤，减少用户态处理开销
- **fd 映射**：`fd_manager_t` 将 POSIX fd 映射为 64 位 ID，避免 epoll 中的 fd 复用冲突

### 平台差异

- **Linux**（默认，`UDP2RAW_LINUX`）：raw socket + BPF + epoll + iptables
- **多平台**（`UDP2RAW_MP`）：libpcap/npcap 抓包，用于 Windows/macOS
- **Android**（`__ANDROID__`）：JNI 桥接（`android/jni_bridge.cpp`），`myexit()` 抛异常替代 `exit()`，`g_plain_udp` 回退到普通 SOCK_DGRAM，日志输出到 logcat

### 内嵌第三方库

- `libev/` — libev 事件循环（完整嵌入）
- `lib/md5.cpp` — MD5
- `lib/pbkdf2-sha1.cpp`, `lib/pbkdf2-sha256.cpp` — PBKDF2 密钥派生
- `lib/aes_faster_c/` — 纯 C++ AES（软件回退）
- `lib/aes_acc/` — 硬件加速 AES（x86 AES-NI、ARM NEON、MIPS 汇编）

## 注意事项

- 创建 commit 时不要包含 `Co-Authored-By` 信息
- 全局变量定义在 `misc.h` 中声明，`misc.cpp` 中定义。`common.h` 包含核心类型和平台检测宏
- `encrypt.cpp` 中加密密钥是方向性的（发送和接收使用不同密钥）
- 无正式测试框架；`unit_test()` 在 `misc.h` 中声明但基本为占位符
- Android 全局状态需通过 `reset_udp2raw_globals()` 重置以支持 JNI 重启
- `git_version.h` 由 `make git_version` 目标自动生成，不要手动编辑
