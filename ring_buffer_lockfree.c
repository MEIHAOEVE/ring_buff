/**
 * @file    ring_buffer_lockfree.c
 * @brief   环形缓冲区无锁实现
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.1
 * 
 * @details
 * 适用场景：
 * - 单生产者单消费者（SPSC）
 * - ISR → 主循环
 * - DMA 回调 → 任务处理
 * 
 * 线程安全保证：
 * - 无需加锁，依赖内存顺序保证
 * - 生产者只修改 head，消费者只修改 tail
 * 
 * @warning 禁止多个生产者或多个消费者同时访问
 */

#include "ring_buffer.h"

#if RING_BUFFER_ENABLE_LOCKFREE

/* Private functions ---------------------------------------------------------*/

static inline uint16_t lockfree_available_internal(const ring_buffer_t *rb)
{
    uint16_t head = rb->head;
    uint16_t tail = rb->tail;
    
    if (head >= tail) {
        return head - tail;
    } else {
        return rb->size - tail + head;
    }
}

static inline uint16_t lockfree_free_space_internal(const ring_buffer_t *rb)
{
    return rb->size - 1 - lockfree_available_internal(rb);
}

/* Exported functions (Implementation) ---------------------------------------*/

static bool lockfree_write(ring_buffer_t *rb, uint8_t data)
{
    uint16_t next_head = (rb->head + 1) % rb->size;
    
    if (next_head == rb->tail) {
#if RING_BUFFER_ENABLE_STATISTICS
        rb->overflow_count++;
#endif
        return false;  /* 满 */
    }
    
    rb->buffer[rb->head] = data;
    rb->head = next_head;
    
#if RING_BUFFER_ENABLE_STATISTICS
    rb->write_count++;
#endif
    
    return true;
}

static bool lockfree_read(ring_buffer_t *rb, uint8_t *data)
{
    if (rb->tail == rb->head) {
        return false;  /* 空 */
    }
    
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->size;
    
#if RING_BUFFER_ENABLE_STATISTICS
    rb->read_count++;
#endif
    
    return true;
}

static uint16_t lockfree_write_multi(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
    uint16_t free = lockfree_free_space_internal(rb);
    uint16_t to_write = (len > free) ? free : len;
    
    if (to_write == 0) {
#if RING_BUFFER_ENABLE_STATISTICS
        if (len > 0) rb->overflow_count++;
#endif
        return 0;
    }
    
    uint16_t head = rb->head;
    uint16_t size = rb->size;
    
    if (head + to_write <= size) {
        memcpy(&rb->buffer[head], data, to_write);
        rb->head = (head + to_write) % size;
    } else {
        uint16_t first_chunk = size - head;
        uint16_t second_chunk = to_write - first_chunk;
        
        memcpy(&rb->buffer[head], data, first_chunk);
        memcpy(&rb->buffer[0], &data[first_chunk], second_chunk);
        
        rb->head = second_chunk;
    }
    
#if RING_BUFFER_ENABLE_STATISTICS
    rb->write_count += to_write;
    if (to_write < len) rb->overflow_count++;
#endif
    
    return to_write;
}

static uint16_t lockfree_read_multi(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    uint16_t available = lockfree_available_internal(rb);
    uint16_t to_read = (len > available) ? available : len;
    
    if (to_read == 0) {
        return 0;
    }
    
    uint16_t tail = rb->tail;
    uint16_t size = rb->size;
    
    if (tail + to_read <= size) {
        memcpy(data, &rb->buffer[tail], to_read);
        rb->tail = (tail + to_read) % size;
    } else {
        uint16_t first_chunk = size - tail;
        uint16_t second_chunk = to_read - first_chunk;
        
        memcpy(data, &rb->buffer[tail], first_chunk);
        memcpy(&data[first_chunk], &rb->buffer[0], second_chunk);
        
        rb->tail = second_chunk;
    }
    
#if RING_BUFFER_ENABLE_STATISTICS
    rb->read_count += to_read;
#endif
    
    return to_read;
}

static uint16_t lockfree_available(const ring_buffer_t *rb)
{
    return lockfree_available_internal(rb);
}

static uint16_t lockfree_free_space(const ring_buffer_t *rb)
{
    return lockfree_free_space_internal(rb);
}

static bool lockfree_is_empty(const ring_buffer_t *rb)
{
    return (rb->head == rb->tail);
}

static bool lockfree_is_full(const ring_buffer_t *rb)
{
    return ((rb->head + 1) % rb->size == rb->tail);
}

static void lockfree_clear(ring_buffer_t *rb)
{
    rb->tail = rb->head;
    
#if RING_BUFFER_ENABLE_STATISTICS
    rb->write_count = 0;
    rb->read_count = 0;
    rb->overflow_count = 0;
#endif
}

/* Exported constant ---------------------------------------------------------*/

const struct ring_buffer_ops ring_buffer_lockfree_ops = {
    .write       = lockfree_write,
    .read        = lockfree_read,
    .write_multi = lockfree_write_multi,
    .read_multi  = lockfree_read_multi,
    .available   = lockfree_available,
    .free_space  = lockfree_free_space,
    .is_empty    = lockfree_is_empty,
    .is_full     = lockfree_is_full,
    .clear       = lockfree_clear,
};

#endif /* RING_BUFFER_ENABLE_LOCKFREE */
