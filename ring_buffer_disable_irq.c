/**
 * @file    ring_buffer_disable_irq.c
 * @brief   环形缓冲区关中断实现（临界区保护）
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.0
 * 
 * @details
 * 适用场景：
 * - 裸机系统（无 RTOS）
 * - 多任务但无互斥锁支持
 * - 中断与主循环之间的多生产者/多消费者
 * 
 * 线程安全保证：
 * - 通过关闭中断创建临界区
 * - 适用于 Cortex-M 等单核 MCU
 * 
 * @warning
 * - 会增加中断延迟（批量操作时尤其明显）
 * - 不适用于长时间操作（应分批处理）
 * - 不适用于多核系统
 */

#include "ring_buffer.h"

#if RING_BUFFER_ENABLE_DISABLE_IRQ

/* Platform-specific interrupt control --------------------------------------------------*/

/**
 * @brief 平台相关的中断控制宏
 * 
 * 根据具体 MCU 修改以下宏定义：
 * - Cortex-M (CMSIS): 使用 __disable_irq() / __enable_irq()
 * - AVR: 使用 cli() / sei()
 * - 其他平台: 参考对应 SDK 文档
 */

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_MAIN__)
    /* Cortex-M3/M4/M7/M33 (使用 CMSIS) */
    #include "core_cm4.h"  /* 或 core_cm3.h, core_cm7.h 等 */
    
    #define IRQ_DISABLE()   __disable_irq()
    #define IRQ_ENABLE()    __enable_irq()
    
    /* 保存/恢复中断状态（更安全的嵌套实现） */
    typedef uint32_t irq_state_t;
    #define IRQ_SAVE(state)    do { state = __get_PRIMASK(); __disable_irq(); } while(0)
    #define IRQ_RESTORE(state) do { __set_PRIMASK(state); } while(0)

#elif defined(__AVR__)
    /* AVR (Arduino 等) */
    #include <avr/interrupt.h>
    
    typedef uint8_t irq_state_t;
    #define IRQ_SAVE(state)    do { state = SREG; cli(); } while(0)
    #define IRQ_RESTORE(state) do { SREG = state; } while(0)

#else
    /* 默认实现（请根据平台修改） */
    //#warning "Please define IRQ_SAVE/IRQ_RESTORE for your platform"
    
    typedef uint32_t irq_state_t;
    #define IRQ_SAVE(state)    do { state = 0; } while(0)
    #define IRQ_RESTORE(state) do { (void)state; } while(0)
    
#endif

/* Private functions (reuse lockfree logic) ---------------------------------------------*/

/* 复用无锁实现的内部逻辑（核心算法相同，只是加了临界区保护） */
extern const ring_buffer_ops_t ring_buffer_lockfree_ops;

/* Exported functions (Implementation) --------------------------------------------------*/

/**
 * @brief 关中断单字节写入
 */
static bool disable_irq_write(ring_buffer_t *rb, uint8_t data)
{
    if (!rb) return false;
    
    irq_state_t state;
    IRQ_SAVE(state);
    
    /* 调用无锁实现（已在临界区内） */
    bool ret = ring_buffer_lockfree_ops.write(rb, data);
    
    IRQ_RESTORE(state);
    return ret;
}

/**
 * @brief 关中断单字节读取
 */
static bool disable_irq_read(ring_buffer_t *rb, uint8_t *data)
{
    if (!rb || !data) return false;
    
    irq_state_t state;
    IRQ_SAVE(state);
    
    bool ret = ring_buffer_lockfree_ops.read(rb, data);
    
    IRQ_RESTORE(state);
    return ret;
}

/**
 * @brief 关中断批量写入
 */
static uint16_t disable_irq_write_multi(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
    if (!rb || !data || len == 0) return 0;
    
    irq_state_t state;
    IRQ_SAVE(state);
    
    uint16_t ret = ring_buffer_lockfree_ops.write_multi(rb, data, len);
    
    IRQ_RESTORE(state);
    return ret;
}

/**
 * @brief 关中断批量读取
 */
static uint16_t disable_irq_read_multi(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    if (!rb || !data || len == 0) return 0;
    
    irq_state_t state;
    IRQ_SAVE(state);
    
    uint16_t ret = ring_buffer_lockfree_ops.read_multi(rb, data, len);
    
    IRQ_RESTORE(state);
    return ret;
}

/**
 * @brief 查询可用数据量（只读操作，可选加锁）
 * @note 
 * 理论上只读操作无需加锁，但为保证读取 head/tail 的原子性，
 * 这里仍使用临界区。如追求极致性能可去掉加锁。
 */
static uint16_t disable_irq_available(const ring_buffer_t *rb)
{
    if (!rb) return 0;
    
    irq_state_t state;
    IRQ_SAVE(state);
    
    uint16_t ret = ring_buffer_lockfree_ops.available(rb);
    
    IRQ_RESTORE(state);
    return ret;
}

/**
 * @brief 查询剩余空间
 */
static uint16_t disable_irq_free_space(const ring_buffer_t *rb)
{
    if (!rb) return 0;
    
    irq_state_t state;
    IRQ_SAVE(state);
    
    uint16_t ret = ring_buffer_lockfree_ops.free_space(rb);
    
    IRQ_RESTORE(state);
    return ret;
}

/**
 * @brief 判断是否为空
 */
static bool disable_irq_is_empty(const ring_buffer_t *rb)
{
    if (!rb) return true;
    
    irq_state_t state;
    IRQ_SAVE(state);
    
    bool ret = ring_buffer_lockfree_ops.is_empty(rb);
    
    IRQ_RESTORE(state);
    return ret;
}

/**
 * @brief 判断是否已满
 */
static bool disable_irq_is_full(const ring_buffer_t *rb)
{
    if (!rb) return false;
    
    irq_state_t state;
    IRQ_SAVE(state);
    
    bool ret = ring_buffer_lockfree_ops.is_full(rb);
    
    IRQ_RESTORE(state);
    return ret;
}

/**
 * @brief 清空缓冲区
 */
static void disable_irq_clear(ring_buffer_t *rb)
{
    if (!rb) return;
    
    irq_state_t state;
    IRQ_SAVE(state);
    
    ring_buffer_lockfree_ops.clear(rb);
    
    IRQ_RESTORE(state);
}

/* Exported constant --------------------------------------------------------------------*/

/**
 * @brief 关中断实现的操作接口表
 */
const ring_buffer_ops_t ring_buffer_disable_irq_ops = {
    .write       = disable_irq_write,
    .read        = disable_irq_read,
    .write_multi = disable_irq_write_multi,
    .read_multi  = disable_irq_read_multi,
    .available   = disable_irq_available,
    .free_space  = disable_irq_free_space,
    .is_empty    = disable_irq_is_empty,
    .is_full     = disable_irq_is_full,
    .clear       = disable_irq_clear,
};

#endif /* RING_BUFFER_ENABLE_DISABLE_IRQ */

/* ---------------------------------- end of file ------------------------------------- */
