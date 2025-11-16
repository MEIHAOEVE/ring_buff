/**
 * @file    ring_buffer_disable_irq.c
 * @brief   环形缓冲区关中断实现
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.1
 * 
 * @details
 * 适用场景：
 * - 裸机系统（无 RTOS）
 * - 多个中断源共享缓冲区
 * - 中断与多任务之间通信
 * 
 * 线程安全保证：
 * - 通过关闭中断创建临界区
 * - 适用于 Cortex-M 等单核 MCU
 * 
 * @warning
 * - 会增加中断延迟
 * - 不适用于多核系统
 */

#include "ring_buffer.h"

#if RING_BUFFER_ENABLE_DISABLE_IRQ

/* 中断控制宏在 ring_buffer_config.h 中定义 */

/* 复用无锁实现的内部逻辑 */
extern const struct ring_buffer_ops ring_buffer_lockfree_ops;

/* Exported functions (Implementation) ---------------------------------------*/

static bool disable_irq_write(ring_buffer_t *rb, uint8_t data)
{
    irq_state_t state;
    IRQ_SAVE(state);
    
    bool ret = ring_buffer_lockfree_ops.write(rb, data);
    
    IRQ_RESTORE(state);
    return ret;
}

static bool disable_irq_read(ring_buffer_t *rb, uint8_t *data)
{
    irq_state_t state;
    IRQ_SAVE(state);
    
    bool ret = ring_buffer_lockfree_ops.read(rb, data);
    
    IRQ_RESTORE(state);
    return ret;
}

static uint16_t disable_irq_write_multi(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
    irq_state_t state;
    IRQ_SAVE(state);
    
    uint16_t ret = ring_buffer_lockfree_ops.write_multi(rb, data, len);
    
    IRQ_RESTORE(state);
    return ret;
}

static uint16_t disable_irq_read_multi(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    irq_state_t state;
    IRQ_SAVE(state);
    
    uint16_t ret = ring_buffer_lockfree_ops.read_multi(rb, data, len);
    
    IRQ_RESTORE(state);
    return ret;
}

static uint16_t disable_irq_available(const ring_buffer_t *rb)
{
    irq_state_t state;
    IRQ_SAVE(state);
    
    uint16_t ret = ring_buffer_lockfree_ops.available(rb);
    
    IRQ_RESTORE(state);
    return ret;
}

static uint16_t disable_irq_free_space(const ring_buffer_t *rb)
{
    irq_state_t state;
    IRQ_SAVE(state);
    
    uint16_t ret = ring_buffer_lockfree_ops.free_space(rb);
    
    IRQ_RESTORE(state);
    return ret;
}

static bool disable_irq_is_empty(const ring_buffer_t *rb)
{
    irq_state_t state;
    IRQ_SAVE(state);
    
    bool ret = ring_buffer_lockfree_ops.is_empty(rb);
    
    IRQ_RESTORE(state);
    return ret;
}

static bool disable_irq_is_full(const ring_buffer_t *rb)
{
    irq_state_t state;
    IRQ_SAVE(state);
    
    bool ret = ring_buffer_lockfree_ops.is_full(rb);
    
    IRQ_RESTORE(state);
    return ret;
}

static void disable_irq_clear(ring_buffer_t *rb)
{
    irq_state_t state;
    IRQ_SAVE(state);
    
    ring_buffer_lockfree_ops.clear(rb);
    
    IRQ_RESTORE(state);
}

/* Exported constant ---------------------------------------------------------*/

const struct ring_buffer_ops ring_buffer_disable_irq_ops = {
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
