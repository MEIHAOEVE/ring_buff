/**
 * @file    ring_buffer_mutex.c
 * @brief   环形缓冲区互斥锁实现（RTOS 环境）
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.0
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
 * - 可配置超时时间
 * 
 * @warning
 * - 不可在 ISR 中使用（互斥锁会导致阻塞）
 * - 性能低于无锁和关中断方式
 * - 需要 RTOS 支持
 */

#include "ring_buffer.h"

#if RING_BUFFER_ENABLE_MUTEX

/* RTOS Adaptation Layer ----------------------------------------------------------------*/

/**
 * @brief RTOS 互斥锁适配层
 * 
 * 根据使用的 RTOS 修改以下宏定义，默认提供 FreeRTOS 实现。
 * 如使用其他 RTOS，请参考对应 API 文档修改。
 */

/* ------------------------ FreeRTOS (默认) ------------------------ */
#if defined(USE_FREERTOS) || 1  /* 默认使用 FreeRTOS */

#include "FreeRTOS.h"
#include "semphr.h"

typedef SemaphoreHandle_t mutex_t;

#define MUTEX_CREATE()        xSemaphoreCreateMutex()
#define MUTEX_LOCK(m)         xSemaphoreTake((m), portMAX_DELAY)
#define MUTEX_UNLOCK(m)       xSemaphoreGive(m)
#define MUTEX_DELETE(m)       vSemaphoreDelete(m)

/* 超时版本（可选） */
#define MUTEX_LOCK_TIMEOUT(m, ticks)  xSemaphoreTake((m), (ticks))

/* ------------------------ RT-Thread ------------------------ */
#elif defined(RT_USING_MUTEX)

#include "rtthread.h"

typedef rt_mutex_t mutex_t;

#define MUTEX_CREATE()        rt_mutex_create("ring_buf", RT_IPC_FLAG_FIFO)
#define MUTEX_LOCK(m)         rt_mutex_take((m), RT_WAITING_FOREVER)
#define MUTEX_UNLOCK(m)       rt_mutex_release(m)
#define MUTEX_DELETE(m)       rt_mutex_delete(m)

/* ------------------------ μC/OS-III ------------------------ */
#elif defined(OS_VERSION)

#include "os.h"

typedef OS_MUTEX mutex_t;

static inline mutex_t MUTEX_CREATE(void) {
    OS_ERR err;
    OS_MUTEX mutex;
    OSMutexCreate(&mutex, "ring_buf", &err);
    return (err == OS_ERR_NONE) ? mutex : NULL;
}

#define MUTEX_LOCK(m)         do { OS_ERR err; OSMutexPend(&(m), 0, OS_OPT_PEND_BLOCKING, NULL, &err); } while(0)
#define MUTEX_UNLOCK(m)       do { OS_ERR err; OSMutexPost(&(m), OS_OPT_POST_NONE, &err); } while(0)
#define MUTEX_DELETE(m)       do { OS_ERR err; OSMutexDel(&(m), OS_OPT_DEL_ALWAYS, &err); } while(0)

/* ------------------------ 未定义 RTOS ------------------------ */
#else
#error "Please define RTOS mutex adapter macros"
#endif

/* Private functions (reuse lockfree logic) ---------------------------------------------*/

extern const ring_buffer_ops_t ring_buffer_lockfree_ops;

/* Exported functions (for factory) -----------------------------------------------------*/

/**
 * @brief 互斥锁初始化（由工厂函数调用）
 * 
 * @param rb 缓冲区指针
 * @return true=成功, false=创建互斥锁失败
 */
bool ring_buffer_mutex_init(ring_buffer_t *rb)
{
    if (!rb) return false;
    
    /* 创建互斥锁 */
    mutex_t mutex = MUTEX_CREATE();
    if (!mutex) {
        return false;
    }
    
    rb->lock = (void*)mutex;
    return true;
}

/**
 * @brief 互斥锁清理（由 destroy 函数调用）
 */
void ring_buffer_mutex_deinit(ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_DELETE(mutex);
    rb->lock = NULL;
}

/* Exported functions (Implementation) --------------------------------------------------*/

/**
 * @brief 互斥锁单字节写入
 */
static bool mutex_write(ring_buffer_t *rb, uint8_t data)
{
    if (!rb || !rb->lock) return false;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    bool ret = ring_buffer_lockfree_ops.write(rb, data);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

/**
 * @brief 互斥锁单字节读取
 */
static bool mutex_read(ring_buffer_t *rb, uint8_t *data)
{
    if (!rb || !data || !rb->lock) return false;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    bool ret = ring_buffer_lockfree_ops.read(rb, data);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

/**
 * @brief 互斥锁批量写入
 */
static uint16_t mutex_write_multi(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
    if (!rb || !data || len == 0 || !rb->lock) return 0;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    uint16_t ret = ring_buffer_lockfree_ops.write_multi(rb, data, len);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

/**
 * @brief 互斥锁批量读取
 */
static uint16_t mutex_read_multi(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    if (!rb || !data || len == 0 || !rb->lock) return 0;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    uint16_t ret = ring_buffer_lockfree_ops.read_multi(rb, data, len);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

/**
 * @brief 查询可用数据量
 */
static uint16_t mutex_available(const ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return 0;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    uint16_t ret = ring_buffer_lockfree_ops.available(rb);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

/**
 * @brief 查询剩余空间
 */
static uint16_t mutex_free_space(const ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return 0;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    uint16_t ret = ring_buffer_lockfree_ops.free_space(rb);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

/**
 * @brief 判断是否为空
 */
static bool mutex_is_empty(const ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return true;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    bool ret = ring_buffer_lockfree_ops.is_empty(rb);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

/**
 * @brief 判断是否已满
 */
static bool mutex_is_full(const ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return false;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    bool ret = ring_buffer_lockfree_ops.is_full(rb);
    
    MUTEX_UNLOCK(mutex);
    return ret;
}

/**
 * @brief 清空缓冲区
 */
static void mutex_clear(ring_buffer_t *rb)
{
    if (!rb || !rb->lock) return;
    
    mutex_t mutex = (mutex_t)rb->lock;
    MUTEX_LOCK(mutex);
    
    ring_buffer_lockfree_ops.clear(rb);
    
    MUTEX_UNLOCK(mutex);
}

/* Exported constant --------------------------------------------------------------------*/

/**
 * @brief 互斥锁实现的操作接口表
 */
const ring_buffer_ops_t ring_buffer_mutex_ops = {
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

/* ---------------------------------- end of file ------------------------------------- */
