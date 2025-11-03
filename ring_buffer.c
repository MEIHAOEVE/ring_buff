/**
 * @file    ring_buffer.c
 * @brief   环形缓冲区实现
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 1.0
 */

#include "ring_buffer.h"
#include <string.h>

/**
 * @brief 初始化环形缓冲区
 * @param rb 环形缓冲区指针
 * @param buffer 缓冲区存储空间
 * @param size 缓冲区大小
 * @note 实际可用大小为 size-1
 */
void ring_buffer_init(ring_buffer_t *rb, uint8_t *buffer, uint16_t size)
{
    rb->buffer = buffer;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
}

/**
 * @brief 写入单个字节
 * @param rb 环形缓冲区指针
 * @param data 要写入的数据
 * @return true:成功, false:缓冲区满
 */
bool ring_buffer_write(ring_buffer_t *rb, uint8_t data)
{
    uint16_t next_head = (rb->head + 1) % rb->size;
    
    if (next_head == rb->tail) 
    {
        return false;  /* 缓冲区满 */
    }
    
    rb->buffer[rb->head] = data;
    rb->head = next_head;
    
    return true;
}

/**
 * @brief 读取单个字节
 * @param rb 环形缓冲区指针
 * @param data 数据存储位置
 * @return true:成功, false:缓冲区空
 */
bool ring_buffer_read(ring_buffer_t *rb, uint8_t *data)
{
    if (rb->head == rb->tail) 
	{
        return false;  /* 缓冲区空 */
    }
    
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->size;
    
    return true;
}
/**
 * @brief 获取可用数据量
 * @param rb 环形缓冲区指针
 * @return 可用数据字节数
 * @note 计算缓冲区中已存储的有效数据量（单位：字节）
 */
uint16_t ring_buffer_available(ring_buffer_t *rb)
{
    if (rb->head >= rb->tail) 
	{
        return rb->head - rb->tail;
    } else 
	{
        return rb->size - rb->tail + rb->head;
    }
}

/**
 * @brief 获取剩余空间
 * @param rb 环形缓冲区指针
 * @return 剩余空间字节数
 * @note 计算缓冲区剩余可用空间（单位：字节）
 */
uint16_t ring_buffer_free_space(ring_buffer_t *rb)
{
    return rb->size - ring_buffer_available(rb) - 1;
}

/**
 * @brief 批量写入数据
 * @param rb 环形缓冲区指针
 * @param data 数据指针
 * @param len 数据长度
 * @return 实际写入的字节数
 */
uint16_t ring_buffer_write_multi(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
    if (len == 0) return 0;

    uint16_t free = ring_buffer_free_space(rb);
    if (free == 0) return 0;

    uint16_t to_write = (len > free) ? free : len;

    uint16_t head = rb->head;
    uint16_t tail = rb->tail;
    uint16_t size = rb->size;

    if (head >= tail)
	{
        // 可用空间分为 [head, size) 和 [0, tail)
        uint16_t part1 = size - head;       // 第一段长度
        if (to_write <= part1) 
		{
            // 全部写入第一段
            memcpy(&rb->buffer[head], data, to_write);
            rb->head = (head + to_write) % size;
        } else 
		{
            // 写入第一段 + 第二段
            memcpy(&rb->buffer[head], data, part1);
            memcpy(&rb->buffer[0], data + part1, to_write - part1);
            rb->head = to_write - part1; // 因为 (head + to_write) % size == to_write - part1
        }
    } else 
	{
        // head < tail，可用空间是 [head, tail)
        memcpy(&rb->buffer[head], data, to_write);
        rb->head = head + to_write; // 不会越界，因为 to_write <= tail - head - 1
    }

    return to_write;
}

/**
 * @brief 批量读取数据
 * @param rb 环形缓冲区指针
 * @param data 数据存储位置
 * @param len 要读取的长度
 * @return 实际读取的字节数
 */
uint16_t ring_buffer_read_multi(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    if (len == 0) return 0;

    uint16_t avail = ring_buffer_available(rb);
    if (avail == 0) return 0;

    uint16_t to_read = (len > avail) ? avail : len;

    uint16_t tail = rb->tail;
    uint16_t head = rb->head;
    uint16_t size = rb->size;

    if (tail < head) 
	{
        // 数据连续 [tail, head)
        memcpy(data, &rb->buffer[tail], to_read);
        rb->tail = tail + to_read;
    } else 
	{
        // 数据分两段 [tail, size) + [0, head)
        uint16_t part1 = size - tail;
        if (to_read <= part1) 
		{
            memcpy(data, &rb->buffer[tail], to_read);
            rb->tail = (tail + to_read) % size;
        } else 
		{
            memcpy(data, &rb->buffer[tail], part1);
            memcpy(data + part1, &rb->buffer[0], to_read - part1);
            rb->tail = to_read - part1;
        }
    }

    return to_read;
}

/**
 * @brief 检查缓冲区是否为空
 * @param rb 环形缓冲区指针
 * @return true:空, false:非空
 */
bool ring_buffer_is_empty(ring_buffer_t *rb)
{
    return rb->head == rb->tail;
}

/**
 * @brief 检查缓冲区是否已满
 * @param rb 环形缓冲区指针
 * @return true:满, false:未满
 */
bool ring_buffer_is_full(ring_buffer_t *rb)
{
    return ((rb->head + 1) % rb->size) == rb->tail;
}

/**
 * @brief 清空缓冲区
 * @param rb 环形缓冲区指针
 */
void ring_buffer_clear(ring_buffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

/* 兼容原有接口的操作结构体 */
const ring_buffer_ops_t ring_buffer_ops = {
    .init = ring_buffer_init,
    .write = ring_buffer_write,
    .read = ring_buffer_read,
    .write_multi = ring_buffer_write_multi,
    .read_multi = ring_buffer_read_multi,
    .available = ring_buffer_available,
    .free_space = ring_buffer_free_space,
    .is_empty = ring_buffer_is_empty,
    .is_full = ring_buffer_is_full,
    .clear = ring_buffer_clear
};

/**
 * @brief 获取环形缓冲区操作接口
 * @return 操作接口指针
 */
const ring_buffer_ops_t *ring_buffer_get_ops(void) 
{
    return &ring_buffer_ops;
}
