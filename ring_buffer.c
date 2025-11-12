/**
 * @file    ring_buffer.c
 * @brief   环形缓冲区实现（改进版 - 增强边界检查）
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 1.1
 * @note    1.增加了完整的参数有效性检查和边界保护
 * @note    2.线程安全说明：
 *           - 单写单读场景（如 ISR写+主循环读）：无需额外保护
 *           - 多写多读场景：需要外部互斥锁保护
 *           - head/tail 使用 volatile 确保内存可见性
 *          3.多核系统说明：
 *           - ARM Cortex-M 单核：volatile 足够
 *           - 多核系统：需要外部内存屏障
 *          4.实际可用大小为 size-1
 *           - 例如：size=16 时只能存储 15 个字节
 *           - 例如：size=2 时只能存储 1 个字节
 */

 #include "ring_buffer.h"
 #include <string.h>
 
 /**
  * @brief 初始化环形缓冲区
  * @param rb 环形缓冲区指针
  * @param buffer 缓冲区存储空间
  * @param size 缓冲区大小
  * @return true:成功, false:参数无效
  * @note 实际可用大小为 size-1，size 至少为 2
  */
 bool ring_buffer_init(ring_buffer_t *rb, uint8_t *buffer, uint16_t size)
 {
     /* 参数有效性检查 */
     if (rb == NULL || buffer == NULL || size < 2)
     {
         return false;
     }
     
     rb->buffer = buffer;
     rb->size = size;
     rb->head = 0;
     rb->tail = 0;
     
     return true;
 }
 
 /**
  * @brief 写入单个字节
  * @param rb 环形缓冲区指针
  * @param data 要写入的数据
  * @return true:成功, false:缓冲区满或参数无效
  */
 bool ring_buffer_write(ring_buffer_t *rb, uint8_t data)
 {
     /* 参数检查 */
     if (rb == NULL || rb->buffer == NULL || rb->size < 2)
     {
         return false;
     }
     
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
  * @return true:成功, false:缓冲区空或参数无效
  */
 bool ring_buffer_read(ring_buffer_t *rb, uint8_t *data)
 {
     /* 参数检查 */
     if (rb == NULL || data == NULL || rb->buffer == NULL || rb->size < 2)
     {
         return false;
     }
     
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
  * @return 可用数据字节数，参数无效时返回 0
  * @note 计算缓冲区中已存储的有效数据量（单位：字节）
  */
 uint16_t ring_buffer_available(const ring_buffer_t *rb)
 {
     /* 参数检查 */
     if (rb == NULL || rb->size < 2)
     {
         return 0;
     }
     
     uint16_t head = rb->head;
     uint16_t tail = rb->tail;
     
     if (head >= tail) 
     {
         return head - tail;
     }
     else 
     {
         return rb->size - tail + head;
     }
 }
 
 /**
  * @brief 获取剩余空间
  * @param rb 环形缓冲区指针
  * @return 剩余空间字节数，参数无效时返回 0
  * @note 计算缓冲区剩余可用空间（单位：字节）
  */
 uint16_t ring_buffer_free_space(const ring_buffer_t *rb)
 {
     /* 参数检查 */
     if (rb == NULL || rb->size < 2)
     {
         return 0;
     }
     
     uint16_t avail = ring_buffer_available(rb);
     return rb->size - avail - 1;
 }
 
 /**
  * @brief 批量写入数据
  * @param rb 环形缓冲区指针
  * @param data 数据指针
  * @param len 数据长度
  * @return 实际写入的字节数，参数无效时返回 0
  * @note 在中断环境中使用时：
  *       - 如需原子性，请在调用前关闭中断
  *       - 或使用单字节写入函数
  * @note 使用局部变量快照的原因：
  *       - 避免多次访问 volatile 变量（性能优化）
  *       - 确保整个操作使用一致的状态值（一致性保证）
  *       - 防止在计算过程中被中断修改（避免竞态条件）
  */
 uint16_t ring_buffer_write_multi(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
 {
     /* 参数检查 */
     if (rb == NULL || data == NULL || rb->buffer == NULL || rb->size < 2)
     {
         return 0;
     }
     
     if (len == 0)
     {
         return 0;
     }
 
     uint16_t free = ring_buffer_free_space(rb);
     if (free == 0)
     {
         return 0;
     }
 
     uint16_t to_write = (len > free) ? free : len;
 
     /* 快照当前状态，避免在操作过程中被修改 */
     uint16_t head = rb->head;
     uint16_t tail = rb->tail;
     uint16_t size = rb->size;
 
     if (head >= tail)
     {
         /* 可用空间分为 [head, size) 和 [0, tail) */
         uint16_t part1 = size - head;
         if (to_write <= part1) 
         {
             /* 全部写入第一段 */
             memcpy(&rb->buffer[head], data, to_write);
             rb->head = (head + to_write) % size;
         }
         else 
         {
             /* 写入第一段 + 第二段 */
             memcpy(&rb->buffer[head], data, part1);
             memcpy(&rb->buffer[0], data + part1, to_write - part1);
             rb->head = to_write - part1;
         }
     }
     else 
     {
         /* head < tail，可用空间是 [head, tail) */
         memcpy(&rb->buffer[head], data, to_write);
         rb->head = head + to_write;
     }
 
     return to_write;
 }
 
 /**
  * @brief 批量读取数据
  * @param rb 环形缓冲区指针
  * @param data 数据存储位置
  * @param len 要读取的长度
  * @return 实际读取的字节数，参数无效时返回 0
  * @note 在中断环境中使用时：
  *       - 如需原子性，请在调用前关闭中断
  *       - 或使用单字节读取函数
  * @note 使用局部变量快照的原因：
  *       - 避免多次访问 volatile 变量（性能优化）
  *       - 确保整个操作使用一致的状态值（一致性保证）
  *       - 防止在计算过程中被中断修改（避免竞态条件）
  */
 uint16_t ring_buffer_read_multi(ring_buffer_t *rb, uint8_t *data, uint16_t len)
 {
     /* 参数检查 */
     if (rb == NULL || data == NULL || rb->buffer == NULL || rb->size < 2)
     {
         return 0;
     }
     
     if (len == 0)
     {
         return 0;
     }
 
     uint16_t avail = ring_buffer_available(rb);
     if (avail == 0)
     {
         return 0;
     }
 
     uint16_t to_read = (len > avail) ? avail : len;
 
     /* 快照当前状态，避免在操作过程中被修改 */
     uint16_t tail = rb->tail;
     uint16_t head = rb->head;
     uint16_t size = rb->size;
 
     if (tail < head) 
     {
         /* 数据连续 [tail, head) */
         memcpy(data, &rb->buffer[tail], to_read);
         rb->tail = tail + to_read;
     }
     else 
     {
         /* 数据分两段 [tail, size) + [0, head) */
         uint16_t part1 = size - tail;
         if (to_read <= part1) 
         {
             memcpy(data, &rb->buffer[tail], to_read);
             rb->tail = (tail + to_read) % size;
         }
         else 
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
  * @return true:空或参数无效, false:非空
  */
 bool ring_buffer_is_empty(const ring_buffer_t *rb)
 {
     /* 参数检查 */
     if (rb == NULL)
     {
         return true;  /* 无效缓冲区视为空 */
     }
     
     return rb->head == rb->tail;
 }
 
 /**
  * @brief 检查缓冲区是否已满
  * @param rb 环形缓冲区指针
  * @return true:满, false:未满或参数无效
  */
 bool ring_buffer_is_full(const ring_buffer_t *rb)
 {
     /* 参数检查 */
     if (rb == NULL || rb->size < 2)
     {
         return false;  /* 无效缓冲区视为未满 */
     }
     
     /* 分步计算，避免潜在的除零风险 */
     uint16_t next_head = (rb->head + 1) % rb->size;
     return next_head == rb->tail;
 }
 
 /**
  * @brief 清空缓冲区
  * @param rb 环形缓冲区指针
  */
 void ring_buffer_clear(ring_buffer_t *rb)
 {
     /* 参数检查 */
     if (rb == NULL)
     {
         return;
     }
     
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
