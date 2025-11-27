#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <chrono>

#include "tsl/ordered_map_with_exceptions.h"
#include "tsl/ordered_map_exceptions.h"

using namespace tsl;
using namespace std::chrono_literals;

/**
 * 测试空指针防护机制
 */
TEST(OrderedMapExceptionsTest, NullPointerProtection) {
    // 测试指针类型键的空指针检查
    ordered_map_with_exceptions<const char*, int> om;
    
    // 插入有效指针应该成功
    const char* valid_key = "valid_key";
    auto result = om.insert({valid_key, 42});
    EXPECT_TRUE(result.second);
    EXPECT_EQ(result.first->second, 42);
    
    // 插入空指针应该抛出null_pointer_exception
    EXPECT_THROW(om.insert({nullptr, 99}), null_pointer_exception);
    
    // 查找空指针应该抛出null_pointer_exception
    EXPECT_THROW(om.find(nullptr), null_pointer_exception);
    
    // 使用[]操作符访问空指针应该抛出null_pointer_exception
    EXPECT_THROW(om[nullptr], null_pointer_exception);
    
    // 使用at()访问空指针应该抛出null_pointer_exception
    EXPECT_THROW(om.at(nullptr), null_pointer_exception);
    
    // 擦除空指针应该抛出null_pointer_exception
    EXPECT_THROW(om.erase(nullptr), null_pointer_exception);
}

/**
 * 测试内存管理增强机制
 */
TEST(OrderedMapExceptionsTest, MemoryManagement) {
    // 创建一个带有内存管理的ordered_map
    ordered_map_with_exceptions<int, std::vector<char>> om;
    
    // 设置较小的内存限制
    const std::size_t memory_limit = 10 * 1024; // 10KB
    om.set_memory_limit(memory_limit);
    
    EXPECT_EQ(om.memory_limit(), memory_limit);
    
    // 插入足够多的元素来触发内存限制
    const int element_size = 1024; // 每个元素约1KB
    int inserted_count = 0;
    
    try {
        for (int i = 0; i < 20; ++i) { // 尝试插入20个元素（约20KB）
            std::vector<char> data(element_size, 'a');
            om.insert({i, std::move(data)});
            inserted_count++;
        }
    } catch (const memory_limit_exception&) {
        // 预期会抛出内存限制异常
    } catch (...) {
        // 不应该抛出其他类型的异常
        FAIL() << "Unexpected exception type thrown";
    }
    
    // 应该至少插入了几个元素，但不是全部20个
    EXPECT_GT(inserted_count, 0);
    EXPECT_LT(inserted_count, 20);
    
    // 检查当前内存使用量不超过限制
    EXPECT_LE(om.current_memory_usage(), memory_limit);
    
    // 测试内存碎片率阈值设置
    const float fragmentation_threshold = 0.15f;
    om.set_fragmentation_threshold(fragmentation_threshold);
    EXPECT_EQ(om.fragmentation_threshold(), fragmentation_threshold);
}

/**
 * 测试并发安全机制
 */
TEST(OrderedMapExceptionsTest, ConcurrentSafety) {
    // 创建一个线程安全的ordered_map
    thread_safe_ordered_map<int, std::string> ts_om;
    
    const int num_threads = 5;
    const int num_elements = 100;
    std::vector<std::thread> threads;
    
    // 启动多个线程进行并发插入
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&ts_om, i, num_elements]() {
            for (int j = i * num_elements; j < (i + 1) * num_elements; ++j) {
                try {
                    ts_om.insert({j, "value_" + std::to_string(j)});
                } catch (...) {
                    // 记录异常但不中断测试
                    ADD_FAILURE() << "Exception thrown during concurrent insert";
                }
            }
        });
    }
    
    // 启动多个线程进行并发查找
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&ts_om, num_elements]() {
            for (int j = 0; j < num_elements; ++j) {
                try {
                    auto it = ts_om.find(j);
                    // 查找可能成功或失败，这取决于插入是否已完成
                } catch (...) {
                    // 记录异常但不中断测试
                    ADD_FAILURE() << "Exception thrown during concurrent find";
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 检查最终大小是否正确
    EXPECT_EQ(ts_om.size(), num_threads * num_elements);
}

/**
 * 测试异常信息增强机制
 */
TEST(OrderedMapExceptionsTest, ExceptionInformation) {
    ordered_map_with_exceptions<std::string, int> om;
    om.insert({"test", 42});
    
    try {
        // 触发一个异常
        om.at("non_existent_key");
        FAIL() << "Expected out_of_range_exception was not thrown";
    } catch (const out_of_range_exception& e) {
        // 检查异常基本信息
        EXPECT_NE(std::string(e.what()).find("Key not found"), std::string::npos);
        EXPECT_EQ(e.exception_type(), "out_of_range_exception");
        
        // 检查异常包含位置信息
        EXPECT_FALSE(e.file().empty());
        EXPECT_NE(e.line(), 0);
        
        // 检查异常包含时间戳
        EXPECT_FALSE(e.timestamp().empty());
        
        // 检查异常包含容器状态快照
        EXPECT_NE(e.state_snapshot(), nullptr);
        EXPECT_EQ(e.state_snapshot()->size, 1);
        EXPECT_EQ(e.state_snapshot()->max_size, om.max_size());
        
        // 检查JSON序列化功能
        std::string json = e.to_json();
        EXPECT_NE(json.find("exception_type"), std::string::npos);
        EXPECT_NE(json.find("out_of_range_exception"), std::string::npos);
        EXPECT_NE(json.find("message"), std::string::npos);
        EXPECT_NE(json.find("Key not found"), std::string::npos);
        EXPECT_NE(json.find("state_snapshot"), std::string::npos);
        EXPECT_NE(json.find("size"), std::string::npos);
        EXPECT_NE(json.find("1"), std::string::npos);
    }
}

/**
 * 测试安全序列化/反序列化机制
 */
TEST(OrderedMapExceptionsTest, SafeSerialization) {
    // 创建并填充一个ordered_map
    ordered_map_with_exceptions<std::string, int> om;
    om.insert({"one", 1});
    om.insert({"two", 2});
    om.insert({"three", 3});
    
    const size_t original_size = om.size();
    
    // 使用字符串流进行序列化
    std::stringstream ss;
    
    // 设置超时为5秒，重试策略为3次重试，每次间隔1秒
    detail_ordered_hash::network_retry_strategy retry_strategy(3, 1000ms);
    
    // 安全序列化
    EXPECT_NO_THROW(om.serialize_safe(ss, 5000ms, retry_strategy));
    
    // 创建一个新的ordered_map并反序列化
    ordered_map_with_exceptions<std::string, int> om2;
    ss.seekg(0); // 重置流位置
    
    // 安全反序列化
    EXPECT_NO_THROW(om2.deserialize_safe(ss, 5000ms, retry_strategy));
    
    // 检查反序列化后的内容是否正确
    EXPECT_EQ(om2.size(), original_size);
    EXPECT_EQ(om2.at("one"), 1);
    EXPECT_EQ(om2.at("two"), 2);
    EXPECT_EQ(om2.at("three"), 3);
    
    // 测试损坏的数据应该抛出异常
    std::stringstream corrupted_ss;
    corrupted_ss << "corrupted data";
    corrupted_ss.seekg(0);
    
    EXPECT_THROW(om2.deserialize_safe(corrupted_ss, 5000ms, retry_strategy), network_exception);
}

/**
 * 测试迭代器有效性检查
 */
TEST(OrderedMapExceptionsTest, IteratorValidity) {
    ordered_map_with_exceptions<int, std::string> om;
    
    // 插入一些元素
    for (int i = 0; i < 10; ++i) {
        om.insert({i, "value_" + std::to_string(i)});
    }
    
    // 获取一个有效的迭代器
    auto valid_it = om.find(5);
    EXPECT_NE(valid_it, om.end());
    
    // 测试有效的迭代器操作应该成功
    EXPECT_NO_THROW(om.erase(valid_it));
    
    // 测试已失效的迭代器操作应该抛出异常
    EXPECT_THROW(om.erase(valid_it), invalid_iterator_exception);
    
    // 测试end()迭代器的有效性
    auto end_it = om.end();
    EXPECT_NO_THROW(om.erase(end_it)); // 擦除end()应该是安全的，返回end()
    
    // 测试begin()迭代器的有效性
    auto begin_it = om.begin();
    EXPECT_NO_THROW(om.erase(begin_it)); // 擦除begin()应该成功
}

/**
 * 测试线程安全迭代器
 */
TEST(OrderedMapExceptionsTest, ThreadSafeIterator) {
    thread_safe_ordered_map<int, std::string> ts_om;
    
    // 插入一些元素
    for (int i = 0; i < 5; ++i) {
        ts_om.insert({i, "value_" + std::to_string(i)});
    }
    
    // 使用线程安全迭代器遍历
    int count = 0;
    for (auto it = ts_om.thread_safe_begin(); it != ts_om.thread_safe_end(); ++it) {
        EXPECT_EQ(it->first, count);
        EXPECT_EQ(it->second, "value_" + std::to_string(count));
        count++;
    }
    EXPECT_EQ(count, 5);
    
    // 在另一个线程中修改容器
    std::thread modifier([&ts_om]() {
        ts_om.insert({5, "value_5"});
        ts_om.erase(0);
    });
    
    // 主线程继续使用线程安全迭代器遍历
    count = 0;
    for (auto it = ts_om.thread_safe_begin(); it != ts_om.thread_safe_end(); ++it) {
        count++;
    }
    
    modifier.join();
    
    // 最终大小应该是5（原始5个 - 1个删除 + 1个插入）
    EXPECT_EQ(ts_om.size(), 5);
}

/**
 * 测试兼容性处理
 */
TEST(OrderedMapExceptionsTest, Compatibility) {
    // 测试与标准ordered_map的兼容性
    ordered_map_with_exceptions<std::string, int> om;
    
    // 测试标准API的兼容性
    om["key1"] = 1;
    om["key2"] = 2;
    
    EXPECT_EQ(om.size(), 2);
    EXPECT_EQ(om["key1"], 1);
    EXPECT_EQ(om["key2"], 2);
    
    // 测试迭代器兼容性
    int count = 0;
    for (const auto& pair : om) {
        count++;
    }
    EXPECT_EQ(count, 2);
    
    // 测试交换功能
    ordered_map_with_exceptions<std::string, int> om2;
    om2["key3"] = 3;
    
    om.swap(om2);
    
    EXPECT_EQ(om.size(), 1);
    EXPECT_EQ(om["key3"], 3);
    EXPECT_EQ(om2.size(), 2);
    EXPECT_EQ(om2["key1"], 1);
}

/**
 * 测试异常开关宏
 */
TEST(OrderedMapExceptionsTest, ExceptionMacroSwitch) {
    // 测试异常宏开关的基本功能
    // 注意：这个测试需要手动切换宏定义并重新编译
    
    // 当TSL_ORDERED_MAP_ENABLE_EXCEPTIONS为1时，异常应该被抛出
    ordered_map_with_exceptions<std::string, int> om;
    
#ifdef TSL_ORDERED_MAP_ENABLE_EXCEPTIONS
    EXPECT_THROW(om.at("non_existent"), out_of_range_exception);
#else
    // 当异常被禁用时，应该抛出标准异常或不抛出异常
    EXPECT_THROW(om.at("non_existent"), std::out_of_range);
#endif
}

/**
 * 主测试入口
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
