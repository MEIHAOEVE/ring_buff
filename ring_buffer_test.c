/**
 * @file    ring_buffer_test.c
 * @brief   环形缓冲区测试程序
 * @author  CRITTY.熙影
 * @date    2024-12-27
 * @version 1.1
 */

 #include <stdio.h>
 #include <string.h>
 #include <assert.h>
 #include "ring_buffer.h"
 
 /* 测试用宏定义 */
 #define TEST_BUFFER_SIZE 16
 #define PASS printf("[PASS] ")
 #define FAIL printf("[FAIL] ")
 #define TEST_START(name) printf("\n=== Test: %s ===\n", name)
 
 /* 全局测试计数器 */
 static int tests_passed = 0;
 static int tests_failed = 0;
 
 /* 辅助函数：打印测试结果 */
 void assert_true(bool condition, const char *msg)
 {
     if (condition) {
         PASS;
         printf("%s\n", msg);
         tests_passed++;
     } else {
         FAIL;
         printf("%s\n", msg);
         tests_failed++;
     }
 }
 
 /* 辅助函数：打印缓冲区状态 */
 void print_buffer_state(ring_buffer_t *rb)
 {
     const ring_buffer_ops_t *ops = ring_buffer_get_ops();
     printf("  Buffer State: head=%u, tail=%u, available=%u, free=%u\n",
            rb->head, rb->tail, 
            ops->available(rb),
            ops->free_space(rb));
 }
 
 /* ============================= 测试用例 ============================= */
 
 /**
  * @brief 测试1：初始化功能
  */
 void test_init(void)
 {
     TEST_START("Initialization");
     
     uint8_t buffer[TEST_BUFFER_SIZE];
     ring_buffer_t rb;
     const ring_buffer_ops_t *ops = ring_buffer_get_ops();
     
     /* 测试正常初始化 */
     bool ret = ops->init(&rb, buffer, TEST_BUFFER_SIZE);
     assert_true(ret == true, "Normal initialization should succeed");
     assert_true(rb.size == TEST_BUFFER_SIZE, "Size should be set correctly");
     assert_true(rb.head == 0 && rb.tail == 0, "Pointers should be initialized to 0");
     
     /* 测试空指针 */
     ret = ops->init(NULL, buffer, TEST_BUFFER_SIZE);
     assert_true(ret == false, "NULL rb pointer should fail");
     
     ret = ops->init(&rb, NULL, TEST_BUFFER_SIZE);
     assert_true(ret == false, "NULL buffer pointer should fail");
     
     /* 测试无效大小 */
     ret = ops->init(&rb, buffer, 1);
     assert_true(ret == false, "Size < 2 should fail");
 }
 
 /**
  * @brief 测试2：单字节读写
  */
 void test_single_byte_operations(void)
 {
     TEST_START("Single Byte Read/Write");
     
     uint8_t buffer[TEST_BUFFER_SIZE];
     ring_buffer_t rb;
     const ring_buffer_ops_t *ops = ring_buffer_get_ops();
     
     ops->init(&rb, buffer, TEST_BUFFER_SIZE);
     
     /* 测试写入 */
     bool ret = ops->write(&rb, 0xAA);
     assert_true(ret == true, "First write should succeed");
     assert_true(ops->available(&rb) == 1, "Available should be 1");
     
     /* 测试读取 */
     uint8_t data;
     ret = ops->read(&rb, &data);
     assert_true(ret == true, "Read should succeed");
     assert_true(data == 0xAA, "Read data should match written data");
     assert_true(ops->is_empty(&rb), "Buffer should be empty after read");
     
     /* 测试空缓冲区读取 */
     ret = ops->read(&rb, &data);
     assert_true(ret == false, "Read from empty buffer should fail");
 }
 
 /**
  * @brief 测试3：缓冲区满状态
  */
 void test_buffer_full(void)
 {
     TEST_START("Buffer Full Condition");
     
     uint8_t buffer[TEST_BUFFER_SIZE];
     ring_buffer_t rb;
     const ring_buffer_ops_t *ops = ring_buffer_get_ops();
     
     ops->init(&rb, buffer, TEST_BUFFER_SIZE);
     
     /* 写满缓冲区 (size-1 个字节) */
     int i;
     for (i = 0; i < TEST_BUFFER_SIZE - 1; i++) {
         bool ret = ops->write(&rb, (uint8_t)i);
         if (!ret) break;
     }
     
     assert_true(i == TEST_BUFFER_SIZE - 1, "Should write size-1 bytes");
     assert_true(ops->is_full(&rb), "Buffer should be full");
     assert_true(ops->free_space(&rb) == 0, "Free space should be 0");
     
     /* 尝试再写入 */
     bool ret = ops->write(&rb, 0xFF);
     assert_true(ret == false, "Write to full buffer should fail");
     
     print_buffer_state(&rb);
 }
 
 /**
  * @brief 测试4：批量读写
  */
 void test_multi_byte_operations(void)
 {
     TEST_START("Multi-Byte Read/Write");
     
     uint8_t buffer[TEST_BUFFER_SIZE];
     ring_buffer_t rb;
     const ring_buffer_ops_t *ops = ring_buffer_get_ops();
     
     ops->init(&rb, buffer, TEST_BUFFER_SIZE);
     
     /* 写入测试数据 */
     uint8_t write_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
     uint16_t written = ops->write_multi(&rb, write_data, sizeof(write_data));
     assert_true(written == sizeof(write_data), "Should write all bytes");
     assert_true(ops->available(&rb) == sizeof(write_data), "Available should match written");
     
     /* 读取数据 */
     uint8_t read_data[10];
     uint16_t read_count = ops->read_multi(&rb, read_data, sizeof(read_data));
     assert_true(read_count == sizeof(write_data), "Should read all available bytes");
     assert_true(memcmp(write_data, read_data, sizeof(write_data)) == 0, 
                 "Read data should match written data");
     
     print_buffer_state(&rb);
 }
 
 /**
  * @brief 测试5：环绕写入
  */
 void test_wrap_around(void)
 {
     TEST_START("Wrap Around");
     
     uint8_t buffer[TEST_BUFFER_SIZE];
     ring_buffer_t rb;
     const ring_buffer_ops_t *ops = ring_buffer_get_ops();
     
     ops->init(&rb, buffer, TEST_BUFFER_SIZE);
     
     /* 写入一些数据 */
     uint8_t data1[] = {1, 2, 3, 4, 5};
     ops->write_multi(&rb, data1, sizeof(data1));
     
     /* 读取部分数据，腾出空间 */
     uint8_t temp[3];
     ops->read_multi(&rb, temp, 3);
     
     printf("  After partial read:\n");
     print_buffer_state(&rb);
     
     /* 写入更多数据，触发环绕 */
     uint8_t data2[] = {6, 7, 8, 9, 10, 11, 12};
     uint16_t written = ops->write_multi(&rb, data2, sizeof(data2));
     
     printf("  After wrap-around write:\n");
     print_buffer_state(&rb);
     
     /* 验证数据完整性 */
     uint8_t expected[] = {4, 5, 6, 7, 8, 9, 10, 11, 12};
     uint8_t read_all[20];
     uint16_t total_read = ops->read_multi(&rb, read_all, sizeof(read_all));
     
     assert_true(total_read == sizeof(expected), "Should read expected amount");
     assert_true(memcmp(expected, read_all, sizeof(expected)) == 0,
                 "Wrap-around data should be correct");
 }
 
 /**
  * @brief 测试6：边界条件
  */
 void test_edge_cases(void)
 {
     TEST_START("Edge Cases");
     
     uint8_t buffer[TEST_BUFFER_SIZE];
     ring_buffer_t rb;
     const ring_buffer_ops_t *ops = ring_buffer_get_ops();
     
     ops->init(&rb, buffer, TEST_BUFFER_SIZE);
     
     /* 测试空数据写入 */
     uint16_t ret = ops->write_multi(&rb, (uint8_t*)"test", 0);
     assert_true(ret == 0, "Writing 0 bytes should return 0");
     
     /* 测试空数据读取 */
     uint8_t temp[10];
     ret = ops->read_multi(&rb, temp, 0);
     assert_true(ret == 0, "Reading 0 bytes should return 0");
     
     /* 测试超大写入 */
     uint8_t large_data[100];
     memset(large_data, 0xAA, sizeof(large_data));
     ret = ops->write_multi(&rb, large_data, sizeof(large_data));
     assert_true(ret == TEST_BUFFER_SIZE - 1, 
                 "Should write only available space");
     
     ops->clear(&rb);
     assert_true(ops->is_empty(&rb), "Clear should empty buffer");
 }
 
 /**
  * @brief 测试7：状态查询函数
  */
 void test_status_functions(void)
 {
     TEST_START("Status Query Functions");
     
     uint8_t buffer[TEST_BUFFER_SIZE];
     ring_buffer_t rb;
     const ring_buffer_ops_t *ops = ring_buffer_get_ops();
     
     ops->init(&rb, buffer, TEST_BUFFER_SIZE);
     
     /* 初始状态 */
     assert_true(ops->is_empty(&rb), "New buffer should be empty");
     assert_true(!ops->is_full(&rb), "New buffer should not be full");
     assert_true(ops->available(&rb) == 0, "Available should be 0");
     assert_true(ops->free_space(&rb) == TEST_BUFFER_SIZE - 1, 
                 "Free space should be size-1");
     
     /* 写入后状态 */
     ops->write(&rb, 0x11);
     assert_true(!ops->is_empty(&rb), "Buffer with data should not be empty");
     assert_true(ops->available(&rb) == 1, "Available should be 1");
     assert_true(ops->free_space(&rb) == TEST_BUFFER_SIZE - 2, 
                 "Free space should decrease");
 }
 
 /**
  * @brief 测试8：使用 ops 结构体接口
  */
 void test_ops_interface(void)
 {
     TEST_START("Operations Interface");
     
     uint8_t buffer[TEST_BUFFER_SIZE];
     ring_buffer_t rb;
     const ring_buffer_ops_t *ops = ring_buffer_get_ops();
     
     /* 验证ops指针 */
     assert_true(ops != NULL, "ops pointer should not be NULL");
     assert_true(ops->init != NULL, "init function should not be NULL");
     assert_true(ops->write != NULL, "write function should not be NULL");
     assert_true(ops->read != NULL, "read function should not be NULL");
     
     /* 通过ops接口操作 */
     ops->init(&rb, buffer, TEST_BUFFER_SIZE);
     ops->write(&rb, 0x55);
     
     uint8_t data;
     bool ret = ops->read(&rb, &data);
     assert_true(ret && data == 0x55, "ops interface should work correctly");
 }
 
 /**
  * @brief 测试9：压力测试
  */
 void test_stress(void)
 {
     TEST_START("Stress Test");
     
     uint8_t buffer[TEST_BUFFER_SIZE];
     ring_buffer_t rb;
     const ring_buffer_ops_t *ops = ring_buffer_get_ops();
     
     ops->init(&rb, buffer, TEST_BUFFER_SIZE);
     
     /* 连续读写1000次 */
     int iterations = 1000;
     int errors = 0;
     
     for (int i = 0; i < iterations; i++) {
         uint8_t write_val = (uint8_t)(i & 0xFF);
         
         if (ops->write(&rb, write_val)) {
             uint8_t read_val;
             if (ops->read(&rb, &read_val)) {
                 if (read_val != write_val) {
                     errors++;
                 }
             }
         }
     }
     
     assert_true(errors == 0, "Stress test should have no data errors");
     printf("  Completed %d iterations\n", iterations);
 }
 