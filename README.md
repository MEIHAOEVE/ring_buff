# 🔁 Ring Buffer — 嵌入式系统中间层组件

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C Standard](https://img.shields.io/badge/C-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Version](https://img.shields.io/badge/version-3.0-green.svg)](https://github.com/yourusername/ring_buffer)

高性能、易扩展的环形缓冲区实现，采用**简单工厂模式**设计，适合作为嵌入式系统架构的**中间层组件**。

---

## 📋 目录

- [核心特性](#-核心特性)
- [架构定位](#-架构定位)
- [快速开始](#-快速开始)
- [配置指南](#-配置指南)
- [API 参考](#-api-参考)
- [扩展机制](#-扩展机制)
- [测试与验证](#-测试与验证)
- [常见问题](#-常见问题)
- [设计模式详解](#-设计模式详解)

---

## ✨ 核心特性

### 🏭 简单工厂模式
- **运行时选择策略**：根据 `type` 参数创建不同实现
- **接口简洁清晰**：`ring_buffer_write(&uart_rx_rb, data)`
- **易于扩展**：支持注册自定义策略

### 🔒 三种线程安全策略

| 策略 | 适用场景 | 性能 | 中断延迟 |
|------|----------|------|----------|
| **无锁模式** | ISR → 主循环（SPSC） | ⚡⚡⚡ | 无影响 |
| **关中断模式** | 裸机多任务 | ⚡⚡ | 微秒级 |
| **互斥锁模式** | RTOS 多线程 | ⚡ | RTOS 调度 |

### 🚀 嵌入式友好
- ✅ **完全静态分配**（无堆依赖）
- ✅ **配置与实现分离**（易于移植）
- ✅ **高内聚低耦合**（中间层设计）
- ✅ **零拷贝批量操作**

---

## 🏗️ 架构定位

```
┌─────────────────────────────────────┐
│      应用层（业务逻辑）              │
│   - 协议解析                         │
│   - 数据处理                         │
│   - 状态机                           │
└───────────────┬─────────────────────┘
                │
                ↓
┌─────────────────────────────────────┐
│      中间层（本组件）                │ ← Ring Buffer 在这里
│   - 环形缓冲区                       │
│   - 消息队列                         │
│   - 内存池                           │
└───────────────┬─────────────────────┘
                │
                ↓
┌─────────────────────────────────────┐
│      驱动层（HAL/BSP）               │
│   - UART/SPI/I2C 驱动                │
│   - GPIO/DMA                         │
│   - 中断服务                         │
└─────────────────────────────────────┘
```

**为什么放在中间层？**
1. **解耦应用与驱动**：应用层不直接操作硬件，驱动层不关心业务逻辑
2. **提供通用服务**：多个驱动可共享同一缓冲区管理逻辑
3. **易于测试**：可在 PC 上独立测试缓冲区功能
4. **便于移植**：更换芯片时只需修改驱动层和配置文件

---

## 🚀 快速开始

### 1️⃣ 文件结构

```
ring_buffer/
├── ring_buffer_config.h          # 配置文件（必改）
├── ring_buffer.h                 # 公共头文件
├── ring_buffer.c                 # 工厂函数实现
├── ring_buffer_lockfree.c        # 无锁实现
├── ring_buffer_disable_irq.c     # 关中断实现
├── ring_buffer_mutex.c           # 互斥锁实现
├── ring_buffer_test.c            # 单元测试
└── README.md                     # 本文档
```

### 2️⃣ 基础用法

```c
#include "ring_buffer.h"

int main(void) {
    // 1. 分配资源（全局变量/静态变量）
    static uint8_t uart_rx_buf[256];
    static ring_buffer_t uart_rx_rb;
    
    // 2. 创建缓冲区
    ring_buffer_create(&uart_rx_rb, uart_rx_buf, 256, 
                       RING_BUFFER_TYPE_LOCKFREE);
    
    // 3. 写入数据
    uint8_t tx_data[] = {0x01, 0x02, 0x03, 0x04};
    ring_buffer_write_multi(&uart_rx_rb, tx_data, 4);
    
    // 4. 读取数据
    uint8_t rx_data[10];
    uint16_t read = ring_buffer_read_multi(&uart_rx_rb, rx_data, 10);
    
    // 5. 查询状态
    printf("Available: %u bytes\n", ring_buffer_available(&uart_rx_rb));
    
    // 6. 销毁
    ring_buffer_destroy(&uart_rx_rb);
    
    return 0;
}
```

### 3️⃣ 典型应用场景

#### 场景 1：UART 中断接收（无锁模式）

```c
static uint8_t uart_rx_buf[256];
static ring_buffer_t uart_rx_rb;

void uart_init(void) {
    ring_buffer_create(&uart_rx_rb, uart_rx_buf, 256,
                      RING_BUFFER_TYPE_LOCKFREE);
}

/* ISR：生产者 */
void UART_IRQHandler(void) {
    uint8_t byte = UART->DATA;
    ring_buffer_write(&uart_rx_rb, byte);
}

/* 主循环：消费者 */
void main_loop(void) {
    uint8_t buffer[128];
    while (1) {
        uint16_t len = ring_buffer_read_multi(&uart_rx_rb, buffer, 128);
        if (len > 0) {
            process_data(buffer, len);
        }
    }
}
```

#### 场景 2：多线程日志系统（互斥锁模式）

```c
static uint8_t log_buf[2048];
static ring_buffer_t log_rb;

void log_init(void) {
    ring_buffer_create(&log_rb, log_buf, 2048,
                      RING_BUFFER_TYPE_MUTEX);
}

/* 线程 A：写日志 */
void task_a(void *param) {
    const char *msg = "Task A: Processing...\n";
    ring_buffer_write_multi(&log_rb, (uint8_t*)msg, strlen(msg));
}

/* 线程 B：输出日志 */
void log_output_task(void *param) {
    uint8_t buffer[256];
    uint16_t len = ring_buffer_read_multi(&log_rb, buffer, 256);
    uart_send(buffer, len);
}
```

---

## ⚙️ 配置指南

### 1️⃣ 启用/禁用实现模块

编辑 `ring_buffer_config.h`：

```c
/* 根据项目需求启用 */
#define RING_BUFFER_ENABLE_LOCKFREE    1  // 无锁模式
#define RING_BUFFER_ENABLE_DISABLE_IRQ 1  // 关中断模式
#define RING_BUFFER_ENABLE_MUTEX       1  // 互斥锁模式
```

**建议**：
- 只编译需要的模块，减少代码体积
- 开发阶段全部启用，方便测试

### 2️⃣ 平台适配：中断控制

```c
/* 选择目标平台 */
#define PLATFORM_CORTEX_M   // Cortex-M3/M4/M7/M33
// #define PLATFORM_AVR     // Arduino
// #define PLATFORM_RISCV   // GD32V, ESP32-C3
```

**支持的平台**：
- **Cortex-M**：STM32, NXP i.MX RT, Nordic nRF
- **AVR**：Arduino Uno, Mega
- **RISC-V**：GD32V, ESP32-C3

**自定义平台**：

```c
#define PLATFORM_CUSTOM

typedef uint32_t irq_state_t;

#define IRQ_SAVE(state) do { \
    /* TODO: 保存中断状态并关闭中断 */ \
} while(0)

#define IRQ_RESTORE(state) do { \
    /* TODO: 恢复中断状态 */ \
} while(0)
```

### 3️⃣ RTOS 适配：互斥锁

```c
/* 选择 RTOS */
#define RTOS_FREERTOS      // FreeRTOS
// #define RTOS_RT_THREAD  // RT-Thread
// #define RTOS_UCOS_III   // μC/OS-III
// #define RTOS_THREADX    // ThreadX
```

**支持的 RTOS**：
- FreeRTOS
- RT-Thread
- μC/OS-III
- ThreadX

**自定义 RTOS**：

```c
#define RTOS_CUSTOM

typedef void* mutex_t;

#define MUTEX_CREATE()       /* 创建互斥锁 */
#define MUTEX_LOCK(m)        /* 获取互斥锁 */
#define MUTEX_UNLOCK(m)      /* 释放互斥锁 */
#define MUTEX_DELETE(m)      /* 删除互斥锁 */
#define MUTEX_IS_VALID(m)    /* 判断是否有效 */
```

### 4️⃣ 性能调优

```c
/* 最小缓冲区大小 */
#define RING_BUFFER_MIN_SIZE  2

/* 参数检查（发布版本可禁用） */
#define RING_BUFFER_ENABLE_PARAM_CHECK  1

/* 统计功能（调试用） */
#define RING_BUFFER_ENABLE_STATISTICS  0
```

---

## 📖 API 参考

### 创建与销毁

#### `ring_buffer_create()`
```c
bool ring_buffer_create(
    ring_buffer_t *rb,
    uint8_t *buffer,
    uint16_t size,
    ring_buffer_type_t type
);
```
- **功能**：创建并初始化环形缓冲区
- **参数**：
  - `rb`：控制结构指针（用户分配）
  - `buffer`：数据存储空间（用户分配）
  - `size`：缓冲区大小（实际可用 = size - 1）
  - `type`：策略类型
- **返回值**：`true` = 成功，`false` = 失败

#### `ring_buffer_destroy()`
```c
void ring_buffer_destroy(ring_buffer_t *rb);
```
- **功能**：销毁缓冲区，释放资源

### 基本读写

| 函数 | 功能 | 返回值 |
|------|------|--------|
| `ring_buffer_write()` | 写入单字节 | `bool` |
| `ring_buffer_read()` | 读取单字节 | `bool` |
| `ring_buffer_write_multi()` | 批量写入 | 实际写入字节数 |
| `ring_buffer_read_multi()` | 批量读取 | 实际读取字节数 |

### 状态查询

| 函数 | 功能 | 返回值 |
|------|------|--------|
| `ring_buffer_available()` | 可读数据量 | 字节数 |
| `ring_buffer_free_space()` | 剩余空间 | 字节数 |
| `ring_buffer_is_empty()` | 是否为空 | `bool` |
| `ring_buffer_is_full()` | 是否已满 | `bool` |
| `ring_buffer_clear()` | 清空缓冲区 | 无 |

---

## 🔧 扩展机制

### 为什么需要扩展？

虽然内置三种策略已覆盖大部分场景，但某些特殊需求可能需要自定义实现：
- **调试日志**：记录所有读写操作
- **加密缓冲区**：自动加密/解密数据
- **统计分析**：实时监控缓冲区使用情况
- **特殊同步机制**：如信号量、条件变量

### 扩展步骤

#### 步骤 1：定义自定义策略类型

```c
#define RING_BUFFER_TYPE_CUSTOM_DEBUG  (RING_BUFFER_TYPE_CUSTOM_BASE + 0)
#define RING_BUFFER_TYPE_CUSTOM_CRYPTO (RING_BUFFER_TYPE_CUSTOM_BASE + 1)
```

#### 步骤 2：实现操作接口

```c
/* 自定义实现示例：带日志的写入 */
static bool custom_debug_write(ring_buffer_t *rb, uint8_t data)
{
    printf("[DEBUG] Write: 0x%02X\n", data);
    
    /* 调用无锁实现 */
    extern const struct ring_buffer_ops ring_buffer_lockfree_ops;
    return ring_buffer_lockfree_ops.write(rb, data);
}

static bool custom_debug_read(ring_buffer_t *rb, uint8_t *data)
{
    extern const struct ring_buffer_ops ring_buffer_lockfree_ops;
    bool ret = ring_buffer_lockfree_ops.read(rb, data);
    
    if (ret) {
        printf("[DEBUG] Read: 0x%02X\n", *data);
    }
    
    return ret;
}

/* 定义操作接口表 */
static const struct ring_buffer_ops custom_debug_ops = {
    .write       = custom_debug_write,
    .read        = custom_debug_read,
    /* 其他函数可复用无锁实现 */
    .write_multi = ring_buffer_lockfree_ops.write_multi,
    .read_multi  = ring_buffer_lockfree_ops.read_multi,
    .available   = ring_buffer_lockfree_ops.available,
    .free_space  = ring_buffer_lockfree_ops.free_space,
    .is_empty    = ring_buffer_lockfree_ops.is_empty,
    .is_full     = ring_buffer_lockfree_ops.is_full,
    .clear       = ring_buffer_lockfree_ops.clear,
};
```

#### 步骤 3：注册自定义策略

```c
void system_init(void) {
    /* 注册自定义策略 */
    ring_buffer_register_ops(RING_BUFFER_TYPE_CUSTOM_DEBUG, &custom_debug_ops);
}
```

#### 步骤 4：使用自定义策略

```c
static uint8_t debug_buf[256];
static ring_buffer_t debug_rb;

void debug_buffer_init(void) {
    /* 使用自定义策略创建 */
    ring_buffer_create(&debug_rb, debug_buf, 256, 
                       RING_BUFFER_TYPE_CUSTOM_DEBUG);
}

void test_debug(void) {
    ring_buffer_write(&debug_rb, 0xAA);  // 输出：[DEBUG] Write: 0xAA
    
    uint8_t data;
    ring_buffer_read(&debug_rb, &data);  // 输出：[DEBUG] Read: 0xAA
}
```

### 完整扩展示例：加密缓冲区

```c
/* ==================== 加密策略实现 ==================== */

/* 简单的异或加密 */
#define CRYPTO_KEY  0x5A

static bool crypto_write(ring_buffer_t *rb, uint8_t data)
{
    uint8_t encrypted = data ^ CRYPTO_KEY;  // 加密
    
    extern const struct ring_buffer_ops ring_buffer_lockfree_ops;
    return ring_buffer_lockfree_ops.write(rb, encrypted);
}

static bool crypto_read(ring_buffer_t *rb, uint8_t *data)
{
    extern const struct ring_buffer_ops ring_buffer_lockfree_ops;
    bool ret = ring_buffer_lockfree_ops.read(rb, data);
    
    if (ret) {
        *data ^= CRYPTO_KEY;  // 解密
    }
    
    return ret;
}

static uint16_t crypto_write_multi(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
    /* 临时缓冲区（实际应用中可优化） */
    uint8_t encrypted[256];
    uint16_t chunk_len = (len > 256) ? 256 : len;
    
    /* 加密 */
    for (uint16_t i = 0; i < chunk_len; i++) {
        encrypted[i] = data[i] ^ CRYPTO_KEY;
    }
    
    extern const struct ring_buffer_ops ring_buffer_lockfree_ops;
    return ring_buffer_lockfree_ops.write_multi(rb, encrypted, chunk_len);
}

static uint16_t crypto_read_multi(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    extern const struct ring_buffer_ops ring_buffer_lockfree_ops;
    uint16_t read = ring_buffer_lockfree_ops.read_multi(rb, data, len);
    
    /* 解密 */
    for (uint16_t i = 0; i < read; i++) {
        data[i] ^= CRYPTO_KEY;
    }
    
    return read;
}

/* 定义操作接口表 */
static const struct ring_buffer_ops crypto_ops = {
    .write       = crypto_write,
    .read        = crypto_read,
    .write_multi = crypto_write_multi,
    .read_multi  = crypto_read_multi,
    .available   = ring_buffer_lockfree_ops.available,
    .free_space  = ring_buffer_lockfree_ops.free_space,
    .is_empty    = ring_buffer_lockfree_ops.is_empty,
    .is_full     = ring_buffer_lockfree_ops.is_full,
    .clear       = ring_buffer_lockfree_ops.clear,
};

/* ==================== 使用示例 ==================== */

void crypto_example(void) {
    /* 1. 注册加密策略 */
    ring_buffer_register_ops(RING_BUFFER_TYPE_CUSTOM_CRYPTO, &crypto_ops);
    
    /* 2. 创建加密缓冲区 */
    static uint8_t crypto_buf[256];
    static ring_buffer_t crypto_rb;
    
    ring_buffer_create(&crypto_rb, crypto_buf, 256,
                      RING_BUFFER_TYPE_CUSTOM_CRYPTO);
    
    /* 3. 写入明文（自动加密）*/
    const char *plain = "Hello World";
    ring_buffer_write_multi(&crypto_rb, (uint8_t*)plain, strlen(plain));
    
    /* 4. 读取数据（自动解密）*/
    char buffer[64];
    uint16_t len = ring_buffer_read_multi(&crypto_rb, (uint8_t*)buffer, 64);
    buffer[len] = '\0';
    
    printf("Decrypted: %s\n", buffer);  // 输出：Decrypted: Hello World
}
```

### 扩展注意事项

1. **类型值**：自定义类型必须 >= `RING_BUFFER_TYPE_CUSTOM_BASE`
2. **接口完整性**：所有 9 个函数指针必须有效
3. **线程安全**：根据需求决定是否需要同步机制
4. **注册时机**：必须在创建缓冲区之前注册
5. **最大数量**：默认支持 4 个自定义策略（可修改 `MAX_CUSTOM_OPS`）

---

## 🧪 测试与验证

### 编译测试程序

#### Linux / macOS
```bash
gcc -o test ring_buffer_test.c ring_buffer.c \
    ring_buffer_lockfree.c ring_buffer_disable_irq.c \
    ring_buffer_mutex.c -I. -lpthread

./test
```

#### Windows (MinGW)
```cmd
gcc -o test.exe ring_buffer_test.c ring_buffer.c ^
    ring_buffer_lockfree.c ring_buffer_disable_irq.c ^
    ring_buffer_mutex.c -I.

test.exe
```

### 测试输出示例

```
========== Ring Buffer Unit Tests ==========

✅ PASSED: Create & Destroy
✅ PASSED: Single Byte R/W
✅ PASSED: Multi-Byte R/W
✅ PASSED: Wrap Around
✅ PASSED: Full Condition
✅ PASSED: Clear
  Testing custom strategy:
  [Custom] Writing byte: 0xDE
  [Custom] Writing byte: 0xAD
  [Custom] Read byte: 0xDE
  [Custom] Read byte: 0xAD
✅ PASSED: Custom Strategy

========== All Tests Passed! ==========
```

---

## ❓ 常见问题

### Q1：为什么可用容量 = size - 1？

**答**：这是标准环形缓冲区设计，用于无歧义区分空/满状态。

- **空状态**：`head == tail`
- **满状态**：`(head + 1) % size == tail`
- 如果允许 `head == tail` 表示满，则无法区分空/满

### Q2：如何选择合适的策略？

| 场景 | 推荐策略 | 原因 |
|------|----------|------|
| ISR → 主循环 | 无锁模式 | 性能最高，无中断延迟 |
| 多个 ISR 共享 | 关中断模式 | 简单可靠 |
| RTOS 多线程 | 互斥锁模式 | 支持阻塞等待 |

### Q3：可以在 ISR 中使用互斥锁模式吗？

**答**：**不可以**。互斥锁会导致阻塞，在 ISR 中使用会引发死锁。

### Q4：如何优化批量写入性能？

**答**：
1. 使用 `write_multi()` 而非循环调用 `write()`
2. 关中断模式时分批写入，避免长时间关中断
3. 选择合适的缓冲区大小（避免频繁满状态）

### Q5：如何调试缓冲区溢出？

**答**：
1. 启用统计功能：`#define RING_BUFFER_ENABLE_STATISTICS 1`
2. 定期检查 `rb->overflow_count`
3. 增大缓冲区或优化数据处理速度

---

## 📚 设计模式详解

### 简单工厂模式（本项目采用）

**定义**：由一个工厂函数根据参数创建不同对象。

```c
// 工厂函数
bool ring_buffer_create(ring_buffer_t *rb, ..., ring_buffer_type_t type) {
    switch (type) {
        case LOCKFREE: rb->ops = &lockfree_ops; break;
        case MUTEX: rb->ops = &mutex_ops; break;
    }
}
```

**优点**：
- 简单直接，易于理解
- 适合策略数量固定的场景

**缺点**：
- 新增策略需修改工厂函数（违反开闭原则）
- 本项目通过**注册机制**弥补此缺陷

### 工厂方法模式

**定义**：定义创建对象的接口，由子类决定实例化哪个类。

```c
// 每个策略有自己的工厂
typedef struct {
    ring_buffer_t* (*create)(...);
} factory_t;

factory_t lockfree_factory = { .create = lockfree_create };
factory_t mutex_factory = { .create = mutex_create };

// 使用
ring_buffer_t *rb = lockfree_factory.create(...);
```

**优点**：符合开闭原则  
**缺点**：类数量增多

### 抽象工厂模式

**定义**：创建一系列相关对象的工厂。

```c
// 创建一套通信组件
typedef struct {
    ring_buffer_t* (*create_rx)();
    ring_buffer_t* (*create_tx)();
    dma_t* (*create_dma)();
} comm_factory_t;

// FreeRTOS 通信工厂
comm_factory_t freertos_comm_factory = { ... };

// 裸机通信工厂
comm_factory_t baremetal_comm_factory = { ... };
```

**适用场景**：需要创建多个相关对象

---

## 📄 许可证

MIT License - 详见 [LICENSE](LICENSE) 文件

---

## 📮 联系方式

- **作者**：CRITTY.熙影
- **版本**：2.1 (中间层组件)
- **日期**：2024-12-27

---

**⭐ 如果这个项目对你有帮助，请给个 Star！**