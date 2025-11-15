/**
 * @file    ring_buffer_lockfree.c
 * @brief   环形缓冲区无锁实现（SPSC - 单生产者单消费者）
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.0
 * 
 * @details
 * 适用场景：
 * - ISR 写入 + 主循环读取
 * - DMA 回调 + 用户任务
 * - 任何单写单读的无竞争场景
 * 
 * 线程安全保证：
 * - head 只被生产者修改（写入侧）
 * - tail 只被消费者修改（读取侧）
 * - volatile 确保单核系统的内存可见性
 * 
 * @warning
 * - 多写或多读会导致数据竞争！
 * - 多核系统需配合内存屏障使用
 */

#include "ring_buffer.h"

#if RING_BUFFER_ENABLE_LOCKFREE

/* Private functions --------------------------------------------------------------------*/

/**
 * @brief 计算可用数据量（内部函数，无锁）
 */
static inline uint16_t lockfree_available_internal(const ring_buffer_t *rb)
{
    uint16_t head = rb->head;  /* 原子读取 */
    uint16_t tail = rb->tail;
    
    if (head >= tail) 
	{
        return head - tail;
    } else 
	{
        return rb->size - tail + head;
    }
}

/**
 * @brief 计算剩余空间（内部函数，无锁）
 */
static inline uint16_t lockfree_free_space_internal(const ring_buffer_t *rb)
{
    return rb->size - 1 - lockfree_available_internal(rb);
}

/* Exported functions (Implementation) --------------------------------------------------*/

/**
 * @brief 无锁单字节写入
 */
static bool lockfree_write(ring_buffer_t *rb, uint8_t data)
{
    if (!rb) return false;
    
    uint16_t next_head = (rb->head + 1) % rb->size;
    
    /* 检查是否已满 */
    if (next_head == rb->tail) 
	{
        return false;
    }
    
    /* 写入数据 */
    rb->buffer[rb->head] = data;
    
    /* 更新写指针（volatile 写入） */
    rb->head = next_head;
    
    return true;
}

/**
 * @brief 无锁单字节读取
 */
static bool lockfree_read(ring_buffer_t *rb, uint8_t *data)
{
    if (!rb || !data) return false;
    
    /* 检查是否为空 */
    if (rb->tail == rb->head) 
	{
        return false;
    }
    
    /* 读取数据 */
    *data = rb->buffer[rb->tail];
    
    /* 更新读指针（volatile 写入） */
    rb->tail = (rb->tail + 1) % rb->size;
    
    return true;
}

/**
 * @brief 无锁批量写入
 */
static uint16_t lockfree_write_multi(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
    if (!rb || !data || len == 0) return 0;
    
    uint16_t free = lockfree_free_space_internal(rb);
    uint16_t to_write = (len > free) ? free : len;
    
    if (to_write == 0) return 0;
    
    uint16_t head = rb->head;
    uint16_t size = rb->size;
    
    /* 计算环绕情况 */
    if (head + to_write <= size) 
	{
        /* 一次性写入 */
        memcpy(&rb->buffer[head], data, to_write);
        rb->head = (head + to_write) % size;
    } else 
	{
        /* 分两段写入 */
        uint16_t first_chunk = size - head;
        uint16_t second_chunk = to_write - first_chunk;
        
        memcpy(&rb->buffer[head], data, first_chunk);
        memcpy(&rb->buffer[0], &data[first_chunk], second_chunk);
        
        rb->head = second_chunk;
    }
    
    return to_write;
}

/**
 * @brief 无锁批量读取
 */
static uint16_t lockfree_read_multi(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    if (!rb || !data || len == 0) return 0;
    
    uint16_t available = lockfree_available_internal(rb);
    uint16_t to_read = (len > available) ? available : len;
    
    if (to_read == 0) return 0;
    
    uint16_t tail = rb->tail;
    uint16_t size = rb->size;
    
    /* 计算环绕情况 */
    if (tail + to_read <= size) 
	{
        /* 一次性读取 */
        memcpy(data, &rb->buffer[tail], to_read);
        rb->tail = (tail + to_read) % size;
    } else 
	{
        /* 分两段读取 */
        uint16_t first_chunk = size - tail;
        uint16_t second_chunk = to_read - first_chunk;
        
        memcpy(data, &rb->buffer[tail], first_chunk);
        memcpy(&data[first_chunk], &rb->buffer[0], second_chunk);
        
        rb->tail = second_chunk;
    }
    
    return to_read;
}

/**
 * @brief 查询可用数据量
 */
static uint16_t lockfree_available(const ring_buffer_t *rb)
{
    if (!rb) return 0;
    return lockfree_available_internal(rb);
}

/**
 * @brief 查询剩余空间
 */
static uint16_t lockfree_free_space(const ring_buffer_t *rb)
{
    if (!rb) return 0;
    return lockfree_free_space_internal(rb);
}

/**
 * @brief 判断是否为空
 */
static bool lockfree_is_empty(const ring_buffer_t *rb)
{
    if (!rb) return true;
    return (rb->head == rb->tail);
}

/**
 * @brief 判断是否已满
 */
static bool lockfree_is_full(const ring_buffer_t *rb)
{
    if (!rb) return false;
    return ((rb->head + 1) % rb->size == rb->tail);
}

/**
 * @brief 清空缓冲区
 */
static void lockfree_clear(ring_buffer_t *rb)
{
    if (!rb) return;
    rb->tail = rb->head;  /* 简单重置读指针 */
}

/* Exported constant --------------------------------------------------------------------*/

/**
 * @brief 无锁实现的操作接口表
 */
const ring_buffer_ops_t ring_buffer_lockfree_ops = {
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

/* ---------------------------------- end of file ------------------------------------- */
