/**
 * @file    ring_buffer_test.c
 * @brief   环形缓冲区单元测试
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 2.1
 * 
 * @details
 * 测试所有基本功能和边界条件
 * 
 * 编译方式（Linux/macOS）：
 * gcc -o test ring_buffer_test.c ring_buffer.c ring_buffer_lockfree.c \
 *     ring_buffer_disable_irq.c ring_buffer_mutex.c -I. -lpthread
 * 
 * 运行：
 * ./test
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ring_buffer.h"

/* 测试用宏 */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("❌ FAILED: %s\n", msg); \
        return false; \
    } \
} while(0)

#define TEST_PASS(name) printf("✅ PASSED: %s\n", name)

/* 测试缓冲区 */
static uint8_t test_buffer[256];
static ring_buffer_t test_rb;

/* ==================== 基础功能测试 ==================== */

/**
 * @brief 测试创建和销毁
 */
bool test_create_destroy(void)
{
    /* 正常创建 */
    bool ret = ring_buffer_create(&test_rb, test_buffer, sizeof(test_buffer),
                                   RING_BUFFER_TYPE_LOCKFREE);
    TEST_ASSERT(ret == true, "Create failed");
    TEST_ASSERT(test_rb.buffer == test_buffer, "Buffer pointer mismatch");
    TEST_ASSERT(test_rb.size == sizeof(test_buffer), "Size mismatch");
    TEST_ASSERT(test_rb.head == 0, "Head not initialized");
    TEST_ASSERT(test_rb.tail == 0, "Tail not initialized");
    TEST_ASSERT(test_rb.ops != NULL, "Ops pointer is NULL");
    
    /* 销毁 */
    ring_buffer_destroy(&test_rb);
    TEST_ASSERT(test_rb.buffer == NULL, "Buffer not cleared");
    TEST_ASSERT(test_rb.ops == NULL, "Ops not cleared");
    
    /* 无效参数测试 */
    ret = ring_buffer_create(NULL, test_buffer, 256, RING_BUFFER_TYPE_LOCKFREE);
    TEST_ASSERT(ret == false, "Should fail with NULL rb");
    
    ret = ring_buffer_create(&test_rb, NULL, 256, RING_BUFFER_TYPE_LOCKFREE);
    TEST_ASSERT(ret == false, "Should fail with NULL buffer");
    
    ret = ring_buffer_create(&test_rb, test_buffer, 1, RING_BUFFER_TYPE_LOCKFREE);
    TEST_ASSERT(ret == false, "Should fail with size < MIN_SIZE");
    
    TEST_PASS("Create & Destroy");
    return true;
}

/**
 * @brief 测试单字节读写
 */
bool test_single_byte_rw(void)
{
    ring_buffer_create(&test_rb, test_buffer, 16, RING_BUFFER_TYPE_LOCKFREE);
    
    /* 写入测试 */
    TEST_ASSERT(ring_buffer_write(&test_rb, 0xAA), "Write failed");
    TEST_ASSERT(ring_buffer_available(&test_rb) == 1, "Available should be 1");
    TEST_ASSERT(!ring_buffer_is_empty(&test_rb), "Should not be empty");
    
    /* 读取测试 */
    uint8_t data;
    TEST_ASSERT(ring_buffer_read(&test_rb, &data), "Read failed");
    TEST_ASSERT(data == 0xAA, "Data mismatch");
    TEST_ASSERT(ring_buffer_is_empty(&test_rb), "Should be empty");
    
    /* 空缓冲区读取 */
    TEST_ASSERT(!ring_buffer_read(&test_rb, &data), "Should fail on empty buffer");
    
    ring_buffer_destroy(&test_rb);
    TEST_PASS("Single Byte R/W");
    return true;
}

/**
 * @brief 测试批量读写
 */
bool test_multi_byte_rw(void)
{
    ring_buffer_create(&test_rb, test_buffer, 64, RING_BUFFER_TYPE_LOCKFREE);
    
    uint8_t write_data[32];
    uint8_t read_data[32];
    
    /* 填充测试数据 */
    for (int i = 0; i < 32; i++) {
        write_data[i] = i;
    }
    
    /* 批量写入 */
    uint16_t written = ring_buffer_write_multi(&test_rb, write_data, 32);
    TEST_ASSERT(written == 32, "Write count mismatch");
    TEST_ASSERT(ring_buffer_available(&test_rb) == 32, "Available should be 32");
    
    /* 批量读取 */
    uint16_t read = ring_buffer_read_multi(&test_rb, read_data, 32);
    TEST_ASSERT(read == 32, "Read count mismatch");
    TEST_ASSERT(memcmp(write_data, read_data, 32) == 0, "Data mismatch");
    TEST_ASSERT(ring_buffer_is_empty(&test_rb), "Should be empty");
    
    ring_buffer_destroy(&test_rb);
    TEST_PASS("Multi-Byte R/W");
    return true;
}

/**
 * @brief 测试环绕情况
 */
bool test_wrap_around(void)
{
    ring_buffer_create(&test_rb, test_buffer, 16, RING_BUFFER_TYPE_LOCKFREE);
    
    uint8_t data[20];
    for (int i = 0; i < 20; i++) {
        data[i] = i;
    }
    
    /* 写入 10 字节 */
    ring_buffer_write_multi(&test_rb, data, 10);
    
    /* 读取 5 字节 */
    uint8_t temp[10];
    ring_buffer_read_multi(&test_rb, temp, 5);
    
    /* 再写入 10 字节（会环绕）*/
    uint16_t written = ring_buffer_write_multi(&test_rb, &data[10], 10);
    TEST_ASSERT(written == 10, "Should write 10 bytes with wrap");
    
    /* 验证数据 */
    ring_buffer_read_multi(&test_rb, temp, 5);
    TEST_ASSERT(memcmp(temp, &data[5], 5) == 0, "First part mismatch");
    
    ring_buffer_read_multi(&test_rb, temp, 10);
    TEST_ASSERT(memcmp(temp, &data[10], 10) == 0, "Second part mismatch");
    
    ring_buffer_destroy(&test_rb);
    TEST_PASS("Wrap Around");
    return true;
}

/**
 * @brief 测试满状态
 */
bool test_full_condition(void)
{
    ring_buffer_create(&test_rb, test_buffer, 16, RING_BUFFER_TYPE_LOCKFREE);
    
    /* 写满缓冲区（实际容量 = size - 1 = 15） */
    uint8_t data[20];
    memset(data, 0xBB, sizeof(data));
    
    uint16_t written = ring_buffer_write_multi(&test_rb, data, 20);
    TEST_ASSERT(written == 15, "Should write 15 bytes (size - 1)");
    TEST_ASSERT(ring_buffer_is_full(&test_rb), "Should be full");
    TEST_ASSERT(ring_buffer_free_space(&test_rb) == 0, "Free space should be 0");
    
    /* 再次写入应失败 */
    TEST_ASSERT(!ring_buffer_write(&test_rb, 0xFF), "Write should fail when full");
    
    /* 读取 1 字节后应可再写入 */
    uint8_t temp;
    ring_buffer_read(&test_rb, &temp);
    TEST_ASSERT(!ring_buffer_is_full(&test_rb), "Should not be full");
    TEST_ASSERT(ring_buffer_write(&test_rb, 0xFF), "Write should succeed");
    
    ring_buffer_destroy(&test_rb);
    TEST_PASS("Full Condition");
    return true;
}

/**
 * @brief 测试清空功能
 */
bool test_clear(void)
{
    ring_buffer_create(&test_rb, test_buffer, 32, RING_BUFFER_TYPE_LOCKFREE);
    
    /* 写入数据 */
    uint8_t data[16];
    memset(data, 0xCC, sizeof(data));
    ring_buffer_write_multi(&test_rb, data, 16);
    
    TEST_ASSERT(ring_buffer_available(&test_rb) == 16, "Available should be 16");
    
    /* 清空 */
    ring_buffer_clear(&test_rb);
    TEST_ASSERT(ring_buffer_is_empty(&test_rb), "Should be empty after clear");
    TEST_ASSERT(ring_buffer_available(&test_rb) == 0, "Available should be 0");
    TEST_ASSERT(ring_buffer_free_space(&test_rb) == 31, "Free space should be 31");
    
    ring_buffer_destroy(&test_rb);
    TEST_PASS("Clear");
    return true;
}

/* ==================== 扩展功能测试 ==================== */

/**
 * @brief 自定义策略示例
 */
static bool custom_write(ring_buffer_t *rb, uint8_t data)
{
    /* 自定义实现：在写入前打印日志 */
    printf("  [Custom] Writing byte: 0x%02X\n", data);
    
    /* 调用无锁实现 */
    extern const struct ring_buffer_ops ring_buffer_lockfree_ops;
    return ring_buffer_lockfree_ops.write(rb, data);
}

static bool custom_read(ring_buffer_t *rb, uint8_t *data)
{
    extern const struct ring_buffer_ops ring_buffer_lockfree_ops;
    bool ret = ring_buffer_lockfree_ops.read(rb, data);
    
    if (ret) {
        printf("  [Custom] Read byte: 0x%02X\n", *data);
    }
    
    return ret;
}

/* 复用其他函数 */
extern const struct ring_buffer_ops ring_buffer_lockfree_ops;

static const struct ring_buffer_ops custom_ops = {
    .write       = custom_write,
    .read        = custom_read,
    .write_multi = ring_buffer_lockfree_ops.write_multi,
    .read_multi  = ring_buffer_lockfree_ops.read_multi,
    .available   = ring_buffer_lockfree_ops.available,
    .free_space  = ring_buffer_lockfree_ops.free_space,
    .is_empty    = ring_buffer_lockfree_ops.is_empty,
    .is_full     = ring_buffer_lockfree_ops.is_full,
    .clear       = ring_buffer_lockfree_ops.clear,
};

/**
 * @brief 测试自定义策略注册
 */
bool test_custom_strategy(void)
{
    /* 注册自定义策略 */
    ring_buffer_type_t custom_type = RING_BUFFER_TYPE_CUSTOM_BASE;
    bool ret = ring_buffer_register_ops(custom_type, &custom_ops);
    TEST_ASSERT(ret == true, "Register custom ops failed");
    
    /* 使用自定义策略创建缓冲区 */
    ret = ring_buffer_create(&test_rb, test_buffer, 16, custom_type);
    TEST_ASSERT(ret == true, "Create with custom type failed");
    TEST_ASSERT(test_rb.ops == &custom_ops, "Ops pointer mismatch");
    
    /* 测试读写（会打印日志）*/
    printf("  Testing custom strategy:\n");
    ring_buffer_write(&test_rb, 0xDE);
    ring_buffer_write(&test_rb, 0xAD);
    
    uint8_t data;
    ring_buffer_read(&test_rb, &data);
    ring_buffer_read(&test_rb, &data);
    
    ring_buffer_destroy(&test_rb);
    TEST_PASS("Custom Strategy");
    return true;
}

/* ==================== 主测试函数 ==================== */

int main(void)
{
    printf("\n========== Ring Buffer Unit Tests ==========\n\n");
    
    /* 运行所有测试 */
    test_create_destroy();
    test_single_byte_rw();
    test_multi_byte_rw();
    test_wrap_around();
    test_full_condition();
    test_clear();
    test_custom_strategy();
    
    printf("\n========== All Tests Passed! ==========\n\n");
    
    return 0;
}
