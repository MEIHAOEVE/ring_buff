/**
 * @file    ring_buffer.h
 * @brief   环形缓冲区接口
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 1.0
 */
 
#ifndef __RING_BUFFER_H
#define __RING_BUFFER_H

/* Includes -----------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported types -----------------------------------------------------------------------*/
typedef struct {
    uint8_t *buffer;        // 缓冲区指针 
    volatile uint16_t size; // 缓冲区大小 
    volatile uint16_t head; // 写指针 (write_ptr) 
    volatile uint16_t tail; // 读指针 (read_ptr) 
} ring_buffer_t;

/* Exported functions -------------------------------------------------------------------*/
/* 兼容原有接口 */
typedef struct {
    void (*init)(ring_buffer_t *rb, uint8_t *buffer, uint16_t size);
    bool (*write)(ring_buffer_t *rb, uint8_t data);
    bool (*read)(ring_buffer_t *rb, uint8_t *data);
    uint16_t (*write_multi)(ring_buffer_t *rb, const uint8_t *data, uint16_t len);
    uint16_t (*read_multi)(ring_buffer_t *rb, uint8_t *data, uint16_t len);
    uint16_t (*available)(ring_buffer_t *rb);
    uint16_t (*free_space)(ring_buffer_t *rb);
    bool (*is_empty)(ring_buffer_t *rb);
    bool (*is_full)(ring_buffer_t *rb);
    void (*clear)(ring_buffer_t *rb);
} ring_buffer_ops_t;

/* 获取环形缓冲区操作接口函数 */
const ring_buffer_ops_t *ring_buffer_get_ops(void);

/* ---------------------------------- end of file ------------------------------------- */
#endif 
