/**
 * @file    ring_buffer_config.h
 * @brief   环形缓冲区配置文件 - 统一管理编译选项和平台适配
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.1
 * 
 * @details
 * 本文件用于：
 * 1. 启用/禁用特定实现（减少代码体积）
 * 2. 配置平台相关宏（RTOS、中断控制）
 * 3. 调整性能参数
 * 
 * 使用说明：
 * - 根据项目需求修改本文件
 * - 不要修改 ring_buffer.h 和实现文件
 */

#ifndef __RING_BUFFER_CONFIG_H
#define __RING_BUFFER_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 功能选择 ==================== */

/**
 * @brief 启用的线程安全策略
 * 
 * 根据项目需求启用需要的实现，未启用的代码不会被编译
 * 
 * 选择指南：
 * - LOCKFREE: ISR → 主循环（单生产者单消费者）
 * - DISABLE_IRQ: 裸机多任务，多个中断源共享缓冲区
 * - MUTEX: FreeRTOS/RT-Thread 等 RTOS 多线程
 */
#define RING_BUFFER_ENABLE_LOCKFREE    1  /**< 无锁模式 */
#define RING_BUFFER_ENABLE_DISABLE_IRQ 0  /**< 关中断模式 */
#define RING_BUFFER_ENABLE_MUTEX       0  /**< 互斥锁模式 */

/* ==================== 平台适配：中断控制 ==================== */

#if RING_BUFFER_ENABLE_DISABLE_IRQ

/**
 * @brief 选择目标平台
 * 
 * 取消注释对应的平台宏，或自定义中断控制
 */

/* Cortex-M3/M4/M7/M33 (STM32, NXP i.MX RT, Nordic nRF等) */
#define PLATFORM_CORTEX_M

/* AVR (Arduino Uno, Mega 等) */
// #define PLATFORM_AVR

/* RISC-V (GD32V, ESP32-C3 等) */
// #define PLATFORM_RISCV

/* 自定义平台（需手动实现 IRQ_SAVE/IRQ_RESTORE） */
// #define PLATFORM_CUSTOM

/* -------------------- Cortex-M 实现 -------------------- */
#ifdef PLATFORM_CORTEX_M
    #include "core_cm4.h"  // 或 core_cm3.h, core_cm7.h, core_cm33.h
    
    typedef uint32_t irq_state_t;
    
    #define IRQ_SAVE(state)    do { \
        state = __get_PRIMASK(); \
        __disable_irq(); \
    } while(0)
    
    #define IRQ_RESTORE(state) do { \
        __set_PRIMASK(state); \
    } while(0)

/* -------------------- AVR 实现 -------------------- */
#elif defined(PLATFORM_AVR)
    #include <avr/interrupt.h>
    
    typedef uint8_t irq_state_t;
    
    #define IRQ_SAVE(state)    do { \
        state = SREG; \
        cli(); \
    } while(0)
    
    #define IRQ_RESTORE(state) do { \
        SREG = state; \
    } while(0)

/* -------------------- RISC-V 实现 -------------------- */
#elif defined(PLATFORM_RISCV)
    #include "riscv_encoding.h"
    
    typedef unsigned long irq_state_t;
    
    #define IRQ_SAVE(state)    do { \
        state = read_csr(mstatus); \
        clear_csr(mstatus, MSTATUS_MIE); \
    } while(0)
    
    #define IRQ_RESTORE(state) do { \
        write_csr(mstatus, state); \
    } while(0)

/* -------------------- 自定义平台 -------------------- */
#elif defined(PLATFORM_CUSTOM)
    /* 
     * 请根据目标平台实现以下宏：
     * - irq_state_t: 保存中断状态的类型
     * - IRQ_SAVE(state): 保存当前中断状态并关闭中断
     * - IRQ_RESTORE(state): 恢复之前的中断状态
     */
    #error "请在 ring_buffer_config.h 中实现自定义平台的中断控制"
    
    typedef uint32_t irq_state_t;
    #define IRQ_SAVE(state)    /* TODO: 实现 */
    #define IRQ_RESTORE(state) /* TODO: 实现 */

#else
    #error "未选择目标平台，请在 ring_buffer_config.h 中定义 PLATFORM_xxx"
#endif

#endif /* RING_BUFFER_ENABLE_DISABLE_IRQ */

/* ==================== 平台适配：RTOS 互斥锁 ==================== */

#if RING_BUFFER_ENABLE_MUTEX

/**
 * @brief 选择 RTOS
 * 
 * 取消注释对应的 RTOS 宏，或自定义互斥锁接口
 */

/* FreeRTOS */
#define RTOS_FREERTOS

/* RT-Thread */
// #define RTOS_RT_THREAD

/* μC/OS-III */
// #define RTOS_UCOS_III

/* ThreadX */
// #define RTOS_THREADX

/* 自定义 RTOS */
// #define RTOS_CUSTOM

/* -------------------- FreeRTOS 实现 -------------------- */
#ifdef RTOS_FREERTOS
    #include "FreeRTOS.h"
    #include "semphr.h"
    
    typedef SemaphoreHandle_t mutex_t;
    
    #define MUTEX_CREATE()          xSemaphoreCreateMutex()
    #define MUTEX_LOCK(m)           xSemaphoreTake((m), portMAX_DELAY)
    #define MUTEX_UNLOCK(m)         xSemaphoreGive(m)
    #define MUTEX_DELETE(m)         vSemaphoreDelete(m)
    #define MUTEX_IS_VALID(m)       ((m) != NULL)

/* -------------------- RT-Thread 实现 -------------------- */
#elif defined(RTOS_RT_THREAD)
    #include "rtthread.h"
    
    typedef rt_mutex_t mutex_t;
    
    #define MUTEX_CREATE()          rt_mutex_create("ring_buf", RT_IPC_FLAG_FIFO)
    #define MUTEX_LOCK(m)           rt_mutex_take((m), RT_WAITING_FOREVER)
    #define MUTEX_UNLOCK(m)         rt_mutex_release(m)
    #define MUTEX_DELETE(m)         rt_mutex_delete(m)
    #define MUTEX_IS_VALID(m)       ((m) != RT_NULL)

/* -------------------- μC/OS-III 实现 -------------------- */
#elif defined(RTOS_UCOS_III)
    #include "os.h"
    
    typedef OS_MUTEX mutex_t;
    
    static inline mutex_t MUTEX_CREATE(void) {
        OS_ERR err;
        OS_MUTEX mutex;
        OSMutexCreate(&mutex, "ring_buf", &err);
        return mutex;
    }
    
    #define MUTEX_LOCK(m)           do { \
        OS_ERR err; \
        OSMutexPend(&(m), 0, OS_OPT_PEND_BLOCKING, NULL, &err); \
    } while(0)
    
    #define MUTEX_UNLOCK(m)         do { \
        OS_ERR err; \
        OSMutexPost(&(m), OS_OPT_POST_NONE, &err); \
    } while(0)
    
    #define MUTEX_DELETE(m)         do { \
        OS_ERR err; \
        OSMutexDel(&(m), OS_OPT_DEL_ALWAYS, &err); \
    } while(0)
    
    #define MUTEX_IS_VALID(m)       (1) /* μC/OS-III 无句柄概念 */

/* -------------------- ThreadX 实现 -------------------- */
#elif defined(RTOS_THREADX)
    #include "tx_api.h"
    
    typedef TX_MUTEX mutex_t;
    
    static inline mutex_t MUTEX_CREATE(void) {
        TX_MUTEX mutex;
        tx_mutex_create(&mutex, "ring_buf", TX_NO_INHERIT);
        return mutex;
    }
    
    #define MUTEX_LOCK(m)           tx_mutex_get(&(m), TX_WAIT_FOREVER)
    #define MUTEX_UNLOCK(m)         tx_mutex_put(&(m))
    #define MUTEX_DELETE(m)         tx_mutex_delete(&(m))
    #define MUTEX_IS_VALID(m)       (1)

/* -------------------- 自定义 RTOS -------------------- */
#elif defined(RTOS_CUSTOM)
    /*
     * 请根据目标 RTOS 实现以下宏：
     * - mutex_t: 互斥锁句柄类型
     * - MUTEX_CREATE(): 创建互斥锁
     * - MUTEX_LOCK(m): 获取互斥锁
     * - MUTEX_UNLOCK(m): 释放互斥锁
     * - MUTEX_DELETE(m): 删除互斥锁
     * - MUTEX_IS_VALID(m): 判断互斥锁是否有效
     */
    #error "请在 ring_buffer_config.h 中实现自定义 RTOS 的互斥锁接口"
    
    typedef void* mutex_t;
    #define MUTEX_CREATE()          /* TODO: 实现 */
    #define MUTEX_LOCK(m)           /* TODO: 实现 */
    #define MUTEX_UNLOCK(m)         /* TODO: 实现 */
    #define MUTEX_DELETE(m)         /* TODO: 实现 */
    #define MUTEX_IS_VALID(m)       /* TODO: 实现 */

#else
    #error "未选择 RTOS，请在 ring_buffer_config.h 中定义 RTOS_xxx"
#endif

#endif /* RING_BUFFER_ENABLE_MUTEX */

/* ==================== 性能调优参数 ==================== */

/**
 * @brief 最小缓冲区大小（字节）
 * 
 * 注意：实际可用容量 = size - 1
 */
#ifndef RING_BUFFER_MIN_SIZE
#define RING_BUFFER_MIN_SIZE  2
#endif

/**
 * @brief 是否启用参数检查
 * 
 * 建议：
 * - 开发阶段：启用（1）
 * - 发布版本：禁用（0）以减少代码体积
 */
#ifndef RING_BUFFER_ENABLE_PARAM_CHECK
#define RING_BUFFER_ENABLE_PARAM_CHECK  1
#endif

/**
 * @brief 是否启用统计功能
 * 
 * 启用后可统计读写次数、溢出次数等（调试用）
 */
#ifndef RING_BUFFER_ENABLE_STATISTICS
#define RING_BUFFER_ENABLE_STATISTICS  0
#endif

/* ==================== 调试选项 ==================== */

/**
 * @brief 调试日志宏
 * 
 * 根据项目日志系统修改
 */
#ifdef RING_BUFFER_DEBUG
    #include <stdio.h>
    #define RB_LOG(fmt, ...) printf("[RingBuf] " fmt "\n", ##__VA_ARGS__)
#else
    #define RB_LOG(fmt, ...)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __RING_BUFFER_CONFIG_H */
