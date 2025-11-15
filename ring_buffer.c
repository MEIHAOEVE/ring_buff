/**
 * @file    ring_buffer.c
 * @brief   环形缓冲区工厂函数实现
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.0
 * 
 * @details
 * 实现工厂模式的核心逻辑：
 * 1. 根据 type 参数选择具体实现
 * 2. 初始化公共字段（buffer、size、head、tail）
 * 3. 返回对应实现的操作接口指针
 */

#include "ring_buffer.h"

/* External declarations ----------------------------------------------------------------*/

/* 外部声明各实现模块的操作接口（在对应 .c 文件中定义） */
#if RING_BUFFER_ENABLE_LOCKFREE
extern const ring_buffer_ops_t ring_buffer_lockfree_ops;
#endif

#if RING_BUFFER_ENABLE_DISABLE_IRQ
extern const ring_buffer_ops_t ring_buffer_disable_irq_ops;
#endif

#if RING_BUFFER_ENABLE_MUTEX
extern const ring_buffer_ops_t ring_buffer_mutex_ops;
#endif

/* Private functions --------------------------------------------------------------------*/

/**
 * @brief 公共初始化逻辑（所有实现共用）
 * @param rb     缓冲区指针
 * @param buffer 存储空间指针
 * @param size   缓冲区大小
 * @return true=成功, false=参数无效
 */
static bool ring_buffer_init_common(ring_buffer_t *rb, uint8_t *buffer, uint16_t size)
{
    /* 参数校验 */
    if (!rb || !buffer || size < 2) 
	{
        return false;
    }
    
    /* 初始化公共字段 */
    rb->buffer = buffer;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->lock = NULL;  /* 默认无锁，mutex 模式会覆盖 */
    
    return true;
}

/* Exported functions -------------------------------------------------------------------*/

/**
 * @brief 工厂函数实现
 */
const ring_buffer_ops_t* ring_buffer_create(
    ring_buffer_t *rb,
    uint8_t *buffer,
    uint16_t size,
    ring_buffer_type_t type)
{
    /* 执行公共初始化 */
    if (!ring_buffer_init_common(rb, buffer, size)) 
	{
        return NULL;
    }
    
    /* 根据类型返回对应实现 */
    switch (type) 
	{
        
#if RING_BUFFER_ENABLE_LOCKFREE
        case RING_BUFFER_TYPE_LOCKFREE:
            /* 无锁模式：无需额外初始化 */
            return &ring_buffer_lockfree_ops;
#endif
        
#if RING_BUFFER_ENABLE_DISABLE_IRQ
        case RING_BUFFER_TYPE_DISABLE_IRQ:
            /* 关中断模式：无需额外初始化 */
            return &ring_buffer_disable_irq_ops;
#endif
        
#if RING_BUFFER_ENABLE_MUTEX
        case RING_BUFFER_TYPE_MUTEX:
            /* 互斥锁模式：需创建互斥锁（在 mutex 模块内部完成） */
            {
                /* 调用 mutex 模块的初始化函数创建锁 */
                extern bool ring_buffer_mutex_init(ring_buffer_t *rb);
                if (!ring_buffer_mutex_init(rb)) {
                    return NULL;
                }
            }
            return &ring_buffer_mutex_ops;
#endif
        
        default:
            /* 未知类型或该实现未启用 */
            return NULL;
    }
}

/**
 * @brief 销毁缓冲区
 */
void ring_buffer_destroy(ring_buffer_t *rb, ring_buffer_type_t type)
{
    if (!rb) 
	{
        return;
    }
    
    /* 根据类型执行清理 */
    switch (type) {
        
#if RING_BUFFER_ENABLE_MUTEX
        case RING_BUFFER_TYPE_MUTEX:
            /* 删除互斥锁 */
            {
                extern void ring_buffer_mutex_deinit(ring_buffer_t *rb);
                ring_buffer_mutex_deinit(rb);
            }
            break;
#endif
        
        default:
            /* 其他模式无需特殊清理 */
            break;
    }
    
    /* 清空公共字段 */
    rb->buffer = NULL;
    rb->size = 0;
    rb->head = 0;
    rb->tail = 0;
    rb->lock = NULL;
}

/* ---------------------------------- end of file ------------------------------------- */
