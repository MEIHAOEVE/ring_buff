/**
 * @file    ring_buffer.h
 * @brief   环形缓冲区公共接口 - 中间层组件
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.1
 * 
 * @details
 * 嵌入式系统架构中间层组件，提供高内聚低耦合的环形缓冲区实现
 * 
 * 特性：
 * - 简单工厂模式，运行时选择线程安全策略
 * - 完全静态分配，无堆依赖
 * - 配置与实现分离，易于移植
 * - 支持扩展：通过注册机制添加自定义策略
 * 
 * 架构定位：
 *   应用层（业务逻辑）
 *        ↓
 *   中间层（本组件）← 提供通用缓冲服务
 *        ↓
 *   驱动层（HAL/BSP）
 */

#ifndef __RING_BUFFER_H
#define __RING_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ring_buffer_config.h"

/* Exported types ------------------------------------------------------------*/

/**
 * @brief 线程安全策略枚举
 */
typedef enum {
    RING_BUFFER_TYPE_LOCKFREE = 0,   /**< 无锁模式（SPSC）*/
    RING_BUFFER_TYPE_DISABLE_IRQ,    /**< 关中断模式（裸机）*/
    RING_BUFFER_TYPE_MUTEX,          /**< 互斥锁模式（RTOS）*/
    RING_BUFFER_TYPE_CUSTOM_BASE     /**< 自定义策略起始值 */
} ring_buffer_type_t;

/* Forward declarations ------------------------------------------------------*/
struct ring_buffer_ops;

/**
 * @brief 环形缓冲区控制结构
 * 
 * @note 
 * - 用户可见，便于调试和嵌入其他结构体
 * - 内嵌 ops 指针，无需单独管理
 * - 适合静态分配
 */
typedef struct {
    uint8_t *buffer;                        /**< 数据缓冲区指针 */
    uint16_t size;                          /**< 缓冲区总大小（字节）*/
    volatile uint16_t head;                 /**< 写指针（生产者）*/
    volatile uint16_t tail;                 /**< 读指针（消费者）*/
    void *lock;                             /**< 锁句柄（互斥锁模式）*/
    const struct ring_buffer_ops *ops;      /**< 操作接口指针 */
    
#if RING_BUFFER_ENABLE_STATISTICS
    uint32_t write_count;                   /**< 写入次数 */
    uint32_t read_count;                    /**< 读取次数 */
    uint32_t overflow_count;                /**< 溢出次数 */
#endif
} ring_buffer_t;

/**
 * @brief 操作接口结构体（策略模式）
 * 
 * @note 用户通过封装函数调用，无需直接访问
 */
struct ring_buffer_ops {
    bool (*write)(ring_buffer_t *rb, uint8_t data);
    bool (*read)(ring_buffer_t *rb, uint8_t *data);
    uint16_t (*write_multi)(ring_buffer_t *rb, const uint8_t *data, uint16_t len);
    uint16_t (*read_multi)(ring_buffer_t *rb, uint8_t *data, uint16_t len);
    uint16_t (*available)(const ring_buffer_t *rb);
    uint16_t (*free_space)(const ring_buffer_t *rb);
    bool (*is_empty)(const ring_buffer_t *rb);
    bool (*is_full)(const ring_buffer_t *rb);
    void (*clear)(ring_buffer_t *rb);
};

/* Exported functions --------------------------------------------------------*/

/* ==================== 创建与销毁 ==================== */

/**
 * @brief 创建并初始化环形缓冲区（工厂函数）
 * 
 * @param rb     缓冲区控制结构指针（用户分配）
 * @param buffer 数据存储空间指针（用户分配）
 * @param size   缓冲区大小（字节，必须 >= RING_BUFFER_MIN_SIZE）
 * @param type   线程安全策略
 * 
 * @return true=成功, false=失败
 * 
 * @note 
 * - 完全静态分配，无堆依赖
 * - 实际可用容量 = size - 1
 * - 互斥锁模式会自动创建互斥锁
 * 
 * @code
 * static uint8_t uart_rx_buf[256];
 * static ring_buffer_t uart_rx_rb;
 * 
 * void uart_init(void) {
 *     ring_buffer_create(&uart_rx_rb, uart_rx_buf, 256, 
 *                        RING_BUFFER_TYPE_LOCKFREE);
 * }
 * @endcode
 */
bool ring_buffer_create(
    ring_buffer_t *rb,
    uint8_t *buffer,
    uint16_t size,
    ring_buffer_type_t type
);

/**
 * @brief 销毁环形缓冲区，释放资源
 * 
 * @param rb 缓冲区指针
 * 
 * @note 
 * - 互斥锁模式会删除互斥锁
 * - 不会释放 buffer 内存（由用户管理）
 */
void ring_buffer_destroy(ring_buffer_t *rb);

/* ==================== 基本读写操作 ==================== */

/**
 * @brief 写入单个字节
 */
bool ring_buffer_write(ring_buffer_t *rb, uint8_t data);

/**
 * @brief 读取单个字节
 */
bool ring_buffer_read(ring_buffer_t *rb, uint8_t *data);

/**
 * @brief 批量写入数据
 * 
 * @return 实际写入的字节数（可能小于 len）
 */
uint16_t ring_buffer_write_multi(ring_buffer_t *rb, const uint8_t *data, uint16_t len);

/**
 * @brief 批量读取数据
 * 
 * @return 实际读取的字节数（可能小于 len）
 */
uint16_t ring_buffer_read_multi(ring_buffer_t *rb, uint8_t *data, uint16_t len);

/* ==================== 状态查询 ==================== */

/**
 * @brief 查询可读数据量
 */
uint16_t ring_buffer_available(const ring_buffer_t *rb);

/**
 * @brief 查询剩余空间
 */
uint16_t ring_buffer_free_space(const ring_buffer_t *rb);

/**
 * @brief 判断缓冲区是否为空
 */
bool ring_buffer_is_empty(const ring_buffer_t *rb);

/**
 * @brief 判断缓冲区是否已满
 */
bool ring_buffer_is_full(const ring_buffer_t *rb);

/**
 * @brief 清空缓冲区
 * 
 * @note 仅重置读写指针，不清除实际数据
 */
void ring_buffer_clear(ring_buffer_t *rb);

/* ==================== 扩展机制 ==================== */

/**
 * @brief 注册自定义策略（高级功能）
 * 
 * @param type 策略类型（>= RING_BUFFER_TYPE_CUSTOM_BASE）
 * @param ops  操作接口指针
 * 
 * @return true=成功, false=失败
 * 
 * @note 
 * - 用于扩展新的线程安全策略
 * - 详见 README.md "扩展指南"
 * 
 * @code
 * // 自定义策略示例
 * const struct ring_buffer_ops my_custom_ops = { ... };
 * ring_buffer_register_ops(RING_BUFFER_TYPE_CUSTOM_BASE, &my_custom_ops);
 * @endcode
 */
bool ring_buffer_register_ops(ring_buffer_type_t type, const struct ring_buffer_ops *ops);

/**
 * @brief 获取操作接口指针（性能关键场景）
 * 
 * @return 操作接口指针，参数错误返回 NULL
 * 
 * @warning 
 * - 仅在性能关键场景使用（如 ISR）
 * - 需自行保证参数正确性
 * 
 * @code
 * // ISR 中使用
 * void UART_IRQHandler(void) {
 *     uint8_t byte = UART->DATA;
 *     uart_rx_rb.ops->write(&uart_rx_rb, byte);
 * }
 * @endcode
 */
static inline const struct ring_buffer_ops* ring_buffer_get_ops(const ring_buffer_t *rb)
{
    return (rb ? rb->ops : NULL);
}

#ifdef __cplusplus
}
#endif

#endif /* __RING_BUFFER_H */
