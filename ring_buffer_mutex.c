/**
 * @file    ring_buffer_mutex.c
 * @brief   环形缓冲区互斥锁实现
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.1
 * 
 * @details
 * 适用场景：
 * - FreeRTOS / RT-Thread / μC/OS 等 RTOS 环境
 * - 多线程之间的缓冲区共享
 * - 需要阻塞等待的场景
 * 
 * 线程安全保证：
 * - 使用 RTOS 互斥锁（Mutex）保护
 * - 支持优先级继承（防止优先级反转）
 * 
 * @warning 不可在 ISR 中使用
 */

#include "ring_buffer.h"

#if RING_BUFFER_ENABLE_MUTEX

/* RTOS 互斥锁宏在 ring_buffer_config.h 中定义 */

/* 复用无锁实现的内部逻辑 */
extern const struct ring_buffer_ops ring_buffer_lockfree_ops;

/* Exported functions (for factory) ------------------------------------------*/

bool ring_buffer_mutex_init(ring_buffer_t *rb)
{
    if (!rb) return false;
    
    mutex_t mutex = MUTEX_CREATE();
    if (!MUTEX_IS_VALID(mutex)) {
        return false;
    }
    
    rb->lock = (void*)mutex;
    return true;
}

void ring_buffer_mutex_deinit(ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_DELETE(mutex);
    rb->lock = NULL;
}

/* Exported functions (Implementation) ---------------------------------------*/

static bool mutex_write(ring_buffer_t *rb, uint8_t data)
{
    if (!rb || !rb->lock) return false;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    bool ret = ring_buffer_lockfree_ops.write(rb, data);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

static bool mutex_read(ring_buffer_t *rb, uint8_t *data)
{
    if (!rb || !data || !rb->lock) return false;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    bool ret = ring_buffer_lockfree_ops.read(rb, data);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

static uint16_t mutex_write_multi(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
    if (!rb || !data || len == 0 || !rb->lock) return 0;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    uint16_t ret = ring_buffer_lockfree_ops.write_multi(rb, data, len);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

static uint16_t mutex_read_multi(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    if (!rb || !data || len == 0 || !rb->lock) return 0;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    uint16_t ret = ring_buffer_lockfree_ops.read_multi(rb, data, len);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

static uint16_t mutex_available(const ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return 0;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    uint16_t ret = ring_buffer_lockfree_ops.available(rb);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

static uint16_t mutex_free_space(const ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return 0;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    uint16_t ret = ring_buffer_lockfree_ops.free_space(rb);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

static bool mutex_is_empty(const ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return true;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    bool ret = ring_buffer_lockfree_ops.is_empty(rb);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

static bool mutex_is_full(const ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return false;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    bool ret = ring_buffer_lockfree_ops.is_full(rb);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

static void mutex_clear(ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    ring_buffer_lockfree_ops.clear(rb);
    
    MUTEX_UNLOCK(mutex);
}

/* Exported constant ---------------------------------------------------------*/

const struct ring_buffer_ops ring_buffer_mutex_ops = {
    .write       = mutex_write,
    .read        = mutex_read,
    .write_multi = mutex_write_multi,
    .read_multi  = mutex_read_multi,
    .available   = mutex_available,
    .free_space  = mutex_free_space,
    .is_empty    = mutex_is_empty,
    .is_full     = mutex_is_full,
    .clear       = mutex_clear,
};

#endif /* RING_BUFFER_ENABLE_MUTEX */
