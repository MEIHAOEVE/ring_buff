/**
 * @file    ring_buffer.h
 * @brief   环形缓冲区接口 
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.0
 * 
 * @details 
 * 采用工厂模式设计，支持运行时选择不同的线程安全策略：
 * - 无锁模式 (LOCKFREE)：适用于单生产者/单消费者场景
 * - 关中断模式 (DISABLE_IRQ)：适用于裸机系统临界区保护
 * - 互斥锁模式 (MUTEX)：适用于 RTOS 多线程环境
 * 
 * @note 
 * - 实际可用容量 = size - 1
 * - 所有实现遵循统一的 ring_buffer_ops_t 接口
 */

#ifndef __RING_BUFFER_H
#define __RING_BUFFER_H

/* Includes -----------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Exported defines ---------------------------------------------------------------------*/
/**
 * @brief 条件编译开关：启用/禁用特定实现
 * 
 * 根据实际需求在编译时选择需要的实现，减少代码体积：
 * - 设置为 1：启用该实现
 * - 设置为 0：禁用该实现（对应 .c 文件不会被编译）
 */
#define RING_BUFFER_ENABLE_LOCKFREE    1  /**< 启用无锁实现 */
#define RING_BUFFER_ENABLE_DISABLE_IRQ 1  /**< 启用关中断实现 */
#define RING_BUFFER_ENABLE_MUTEX       0  /**< 启用互斥锁实现 */

/* Exported types -----------------------------------------------------------------------*/

/**
 * @brief 环形缓冲区控制结构
 * 
 * @note 
 * - head 和 tail 声明为 volatile，确保在中断与主程序间的内存可见性
 * - 使用标准的"一个空槽"策略区分空/满状态（实际容量 = size - 1）
 */
typedef struct {
    uint8_t *buffer;        /**< 缓冲区存储空间指针 */
    uint16_t size;          /**< 缓冲区总大小（字节） */
    volatile uint16_t head; /**< 写指针（生产者） */
    volatile uint16_t tail; /**< 读指针（消费者） */
    void *lock;             /**< 锁句柄（互斥锁模式使用，其他模式为 NULL） */
} ring_buffer_t;

/**
 * @brief 缓冲区实现类型枚举
 * 
 * 用于工厂函数 ring_buffer_create() 的 type 参数，指定实例化哪种实现。
 */
typedef enum {
    RING_BUFFER_TYPE_LOCKFREE = 0,   /**< 无锁模式（适用于 SPSC） */
    RING_BUFFER_TYPE_DISABLE_IRQ,    /**< 关中断模式（适用于裸机） */
    RING_BUFFER_TYPE_MUTEX,          /**< 互斥锁模式（适用于 RTOS） */
} ring_buffer_type_t;

/**
 * @brief 环形缓冲区统一操作接口（虚函数表）
 * 
 * 所有实现必须提供此结构的完整实现，上层代码通过此接口调用，
 * 实现了策略模式的解耦。
 */
typedef struct {
    
    /**
     * @brief 写入单个字节
     * @param rb    缓冲区指针
     * @param data  要写入的字节
     * @return true=成功, false=缓冲区已满
     */
    bool (*write)(ring_buffer_t *rb, uint8_t data);
    
    /**
     * @brief 读取单个字节
     * @param rb    缓冲区指针
     * @param data  输出参数，存储读取的字节
     * @return true=成功, false=缓冲区为空
     */
    bool (*read)(ring_buffer_t *rb, uint8_t *data);   
   
    /**
     * @brief 批量写入数据
     * @param rb    缓冲区指针
     * @param data  要写入的数据指针
     * @param len   要写入的字节数
     * @return 实际写入的字节数（可能小于 len）
     * @note 
     * - 自动处理环绕情况（分两段 memcpy）
     * - 空间不足时写入部分数据，返回实际写入量
     */
    uint16_t (*write_multi)(ring_buffer_t *rb, const uint8_t *data, uint16_t len);
    
    /**
     * @brief 批量读取数据
     * @param rb    缓冲区指针
     * @param data  输出缓冲区指针
     * @param len   期望读取的字节数
     * @return 实际读取的字节数（可能小于 len）
     * @note 
     * - 自动处理环绕情况
     * - 数据不足时读取所有可用数据
     */
    uint16_t (*read_multi)(ring_buffer_t *rb, uint8_t *data, uint16_t len);
    
    /**
     * @brief 查询可读数据量
     * @param rb 缓冲区指针
     * @return 可读取的字节数
     */
    uint16_t (*available)(const ring_buffer_t *rb);
    
    /**
     * @brief 查询剩余空间
     * @param rb 缓冲区指针
     * @return 可写入的字节数
     */
    uint16_t (*free_space)(const ring_buffer_t *rb);
    
    /**
     * @brief 判断缓冲区是否为空
     * @param rb 缓冲区指针
     * @return true=空, false=非空
     */
    bool (*is_empty)(const ring_buffer_t *rb);
    
    /**
     * @brief 判断缓冲区是否已满
     * @param rb 缓冲区指针
     * @return true=满, false=未满
     */
    bool (*is_full)(const ring_buffer_t *rb);
    
    /**
     * @brief 清空缓冲区
     * @param rb 缓冲区指针
     * @note 
     * - 仅重置读写指针，不清除实际数据
     * - 互斥锁模式会加锁保护
     */
    void (*clear)(ring_buffer_t *rb);
    
} ring_buffer_ops_t;

/* Exported functions -------------------------------------------------------------------*/

/**
 * @brief 工厂函数：创建并初始化环形缓冲区
 * @param rb     缓冲区控制结构指针（由调用者分配）
 * @param buffer 实际存储空间指针（由调用者分配）
 * @param size   缓冲区大小（必须 >= 2，实际可用 size-1）
 * @param type   实现类型（见 ring_buffer_type_t）
 * @return 
 * - 成功：返回对应实现的操作接口指针
 * - 失败：返回 NULL（参数无效或类型未启用）
 * @note 
 * - 互斥锁模式会动态创建互斥锁（需在 RTOS 环境中）
 * - 如果编译时禁用了某个实现，请求该类型会返回 NULL
 */
const ring_buffer_ops_t* ring_buffer_create(
    ring_buffer_t *rb,
    uint8_t *buffer,
    uint16_t size,
    ring_buffer_type_t type
);

/**
 * @brief 销毁缓冲区（释放资源）
 * 
 * @param rb   缓冲区指针
 * @param type 实现类型（用于确定如何清理资源）
 * @note 
 * - 互斥锁模式会删除互斥锁
 * - 其他模式仅清理内部状态
 * - 不会释放 buffer 内存（由调用者管理）
 */
void ring_buffer_destroy(ring_buffer_t *rb, ring_buffer_type_t type);

/* ---------------------------------- end of file ------------------------------------- */
#endif /* __RING_BUFFER_H */
