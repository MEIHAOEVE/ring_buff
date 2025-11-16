/**
 * @file    ring_buffer.c
 * @brief   环形缓冲区工厂函数与封装实现
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.1
 */

#include "ring_buffer.h"

/* External declarations -----------------------------------------------------*/

#if RING_BUFFER_ENABLE_LOCKFREE
extern const struct ring_buffer_ops ring_buffer_lockfree_ops;
#endif

#if RING_BUFFER_ENABLE_DISABLE_IRQ
extern const struct ring_buffer_ops ring_buffer_disable_irq_ops;
#endif

#if RING_BUFFER_ENABLE_MUTEX
extern const struct ring_buffer_ops ring_buffer_mutex_ops;
extern bool ring_buffer_mutex_init(ring_buffer_t *rb);
extern void ring_buffer_mutex_deinit(ring_buffer_t *rb);
#endif

/* Private defines -----------------------------------------------------------*/
#define MAX_CUSTOM_OPS  4  /**< 最多支持 4 个自定义策略 */

/* Private types -------------------------------------------------------------*/

/**
 * @brief 自定义策略注册表项
 */
typedef struct {
    ring_buffer_type_t type;
    const struct ring_buffer_ops *ops;
} custom_ops_entry_t;

/* Private variables ---------------------------------------------------------*/

/**
 * @brief 自定义策略注册表
 */
static custom_ops_entry_t custom_ops_registry[MAX_CUSTOM_OPS];
static uint8_t custom_ops_count = 0;

/* Private functions ---------------------------------------------------------*/

/**
 * @brief 公共初始化逻辑
 */
static bool ring_buffer_init_common(ring_buffer_t *rb, uint8_t *buffer, uint16_t size)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!rb || !buffer || size < RING_BUFFER_MIN_SIZE) {
        return false;
    }
#endif
    
    rb->buffer = buffer;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->lock = NULL;
    rb->ops = NULL;
    
#if RING_BUFFER_ENABLE_STATISTICS
    rb->write_count = 0;
    rb->read_count = 0;
    rb->overflow_count = 0;
#endif
    
    return true;
}

/**
 * @brief 查找自定义策略
 */
static const struct ring_buffer_ops* find_custom_ops(ring_buffer_type_t type)
{
    for (uint8_t i = 0; i < custom_ops_count; i++) {
        if (custom_ops_registry[i].type == type) {
            return custom_ops_registry[i].ops;
        }
    }
    return NULL;
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 工厂函数：创建环形缓冲区
 */
bool ring_buffer_create(
    ring_buffer_t *rb,
    uint8_t *buffer,
    uint16_t size,
    ring_buffer_type_t type)
{
    /* 公共初始化 */
    if (!ring_buffer_init_common(rb, buffer, size)) {
        RB_LOG("Create failed: invalid parameters");
        return false;
    }
    
    /* 根据类型选择实现 */
    switch (type) {
        
#if RING_BUFFER_ENABLE_LOCKFREE
        case RING_BUFFER_TYPE_LOCKFREE:
            rb->ops = &ring_buffer_lockfree_ops;
            RB_LOG("Created lockfree buffer (size=%u)", size);
            return true;
#endif
        
#if RING_BUFFER_ENABLE_DISABLE_IRQ
        case RING_BUFFER_TYPE_DISABLE_IRQ:
            rb->ops = &ring_buffer_disable_irq_ops;
            RB_LOG("Created disable_irq buffer (size=%u)", size);
            return true;
#endif
        
#if RING_BUFFER_ENABLE_MUTEX
        case RING_BUFFER_TYPE_MUTEX:
            if (!ring_buffer_mutex_init(rb)) {
                RB_LOG("Create failed: mutex init failed");
                return false;
            }
            rb->ops = &ring_buffer_mutex_ops;
            RB_LOG("Created mutex buffer (size=%u)", size);
            return true;
#endif
        
        default:
            /* 尝试查找自定义策略 */
            if (type >= RING_BUFFER_TYPE_CUSTOM_BASE) {
                const struct ring_buffer_ops *custom_ops = find_custom_ops(type);
                if (custom_ops) {
                    rb->ops = custom_ops;
                    RB_LOG("Created custom buffer (type=%d, size=%u)", type, size);
                    return true;
                }
            }
            
            RB_LOG("Create failed: unsupported type %d", type);
            return false;
    }
}

/**
 * @brief 销毁缓冲区
 */
void ring_buffer_destroy(ring_buffer_t *rb)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!rb) {
        return;
    }
#endif
    
#if RING_BUFFER_ENABLE_MUTEX
    /* 清理互斥锁 */
    if (rb->lock) {
        ring_buffer_mutex_deinit(rb);
    }
#endif
    
    RB_LOG("Destroyed buffer");
    
    /* 清空结构体 */
    rb->buffer = NULL;
    rb->size = 0;
    rb->head = 0;
    rb->tail = 0;
    rb->lock = NULL;
    rb->ops = NULL;
}

/**
 * @brief 注册自定义策略
 */
bool ring_buffer_register_ops(ring_buffer_type_t type, const struct ring_buffer_ops *ops)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!ops || type < RING_BUFFER_TYPE_CUSTOM_BASE) {
        RB_LOG("Register failed: invalid parameters");
        return false;
    }
    
    if (custom_ops_count >= MAX_CUSTOM_OPS) {
        RB_LOG("Register failed: registry full");
        return false;
    }
    
    /* 检查是否已注册 */
    if (find_custom_ops(type)) {
        RB_LOG("Register failed: type %d already registered", type);
        return false;
    }
#endif
    
    /* 添加到注册表 */
    custom_ops_registry[custom_ops_count].type = type;
    custom_ops_registry[custom_ops_count].ops = ops;
    custom_ops_count++;
    
    RB_LOG("Registered custom ops (type=%d)", type);
    return true;
}

/* ==================== 便捷封装 API 实现 ==================== */

bool ring_buffer_write(ring_buffer_t *rb, uint8_t data)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!rb || !rb->ops || !rb->ops->write) {
        return false;
    }
#endif
    return rb->ops->write(rb, data);
}

bool ring_buffer_read(ring_buffer_t *rb, uint8_t *data)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!rb || !data || !rb->ops || !rb->ops->read) {
        return false;
    }
#endif
    return rb->ops->read(rb, data);
}

uint16_t ring_buffer_write_multi(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!rb || !data || len == 0 || !rb->ops || !rb->ops->write_multi) {
        return 0;
    }
#endif
    return rb->ops->write_multi(rb, data, len);
}

uint16_t ring_buffer_read_multi(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!rb || !data || len == 0 || !rb->ops || !rb->ops->read_multi) {
        return 0;
    }
#endif
    return rb->ops->read_multi(rb, data, len);
}

uint16_t ring_buffer_available(const ring_buffer_t *rb)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!rb || !rb->ops || !rb->ops->available) {
        return 0;
    }
#endif
    return rb->ops->available(rb);
}

uint16_t ring_buffer_free_space(const ring_buffer_t *rb)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!rb || !rb->ops || !rb->ops->free_space) {
        return 0;
    }
#endif
    return rb->ops->free_space(rb);
}

bool ring_buffer_is_empty(const ring_buffer_t *rb)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!rb || !rb->ops || !rb->ops->is_empty) {
        return true;
    }
#endif
    return rb->ops->is_empty(rb);
}

bool ring_buffer_is_full(const ring_buffer_t *rb)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!rb || !rb->ops || !rb->ops->is_full) {
        return false;
    }
#endif
    return rb->ops->is_full(rb);
}

void ring_buffer_clear(ring_buffer_t *rb)
{
#if RING_BUFFER_ENABLE_PARAM_CHECK
    if (!rb || !rb->ops || !rb->ops->clear) {
        return;
    }
#endif
    rb->ops->clear(rb);
}
