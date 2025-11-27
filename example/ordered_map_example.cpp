#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <stdexcept>
#include <sstream>

#include "tsl/ordered_map_with_exceptions.h"
#include "tsl/ordered_map_exceptions.h"

using namespace tsl;
using namespace std::chrono_literals;

/**
 * 示例：基本使用和异常处理
 */
void basic_usage_example() {
    std::cout << "=== Basic Usage Example ===\n";
    
    try {
        // 创建一个带有异常防护的ordered_map
        ordered_map_with_exceptions<std::string, int> om;
        
        // 设置内存使用上限为1MB
        om.set_memory_limit(1024 * 1024);
        
        // 插入元素
        om.insert({"apple", 1});
        om.insert({"banana", 2});
        om.insert({"cherry", 3});
        
        std::cout << "Size after insertions: " << om.size() << std::endl;
        
        // 访问元素
        std::cout << "apple: " << om.at("apple") << std::endl;
        std::cout << "banana: " << om["banana"] << std::endl;
        
        // 查找元素
        auto it = om.find("cherry");
        if (it != om.end()) {
            std::cout << "cherry found: " << it->second << std::endl;
        }
        
        // 擦除元素
        om.erase("banana");
        std::cout << "Size after erase: " << om.size() << std::endl;
        
        // 尝试访问不存在的元素
        try {
            std::cout << "grape: " << om.at("grape") << std::endl;
        } catch (const out_of_range_exception& e) {
            std::cout << "Caught expected exception: " << e.what() << std::endl;
            std::cout << "Exception details: " << e.to_json() << std::endl;
        }
        
    } catch (const ordered_map_exception& e) {
        std::cout << "Caught ordered_map_exception: " << e.what() << std::endl;
        std::cout << "Exception type: " << e.exception_type() << std::endl;
        std::cout << "Exception details: " << e.to_json() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught std::exception: " << e.what() << std::endl;
    }
    
    std::cout << std::endl;
}

/**
 * 示例：空指针防护
 */
void null_pointer_protection_example() {
    std::cout << "=== Null Pointer Protection Example ===\n";
    
    try {
        // 创建一个键为指针类型的ordered_map
        ordered_map_with_exceptions<const char*, int> om;
        
        const char* valid_key = "valid_key";
        om.insert({valid_key, 42});
        std::cout << "Valid key inserted successfully" << std::endl;
        
        // 尝试插入空指针键（应该抛出异常）
        try {
            om.insert({nullptr, 99});
            std::cout << "ERROR: Null pointer insertion should have failed!" << std::endl;
        } catch (const null_pointer_exception& e) {
            std::cout << "Caught expected null_pointer_exception: " << e.what() << std::endl;
            std::cout << "Exception details: " << e.to_json() << std::endl;
        }
        
        // 尝试查找空指针键（应该抛出异常）
        try {
            auto it = om.find(nullptr);
            std::cout << "ERROR: Null pointer find should have failed!" << std::endl;
        } catch (const null_pointer_exception& e) {
            std::cout << "Caught expected null_pointer_exception: " << e.what() << std::endl;
        }
        
    } catch (const ordered_map_exception& e) {
        std::cout << "Caught ordered_map_exception: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught std::exception: " << e.what() << std::endl;
    }
    
    std::cout << std::endl;
}

/**
 * 示例：并发安全
 */
void concurrent_safety_example() {
    std::cout << "=== Concurrent Safety Example ===\n";
    
    try {
        // 创建一个线程安全的ordered_map
        thread_safe_ordered_map<int, std::string> ts_om;
        
        const int num_threads = 10;
        const int num_elements = 1000;
        std::vector<std::thread> threads;
        
        // 启动多个线程进行并发插入
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&ts_om, i, num_elements]() {
                for (int j = i * num_elements; j < (i + 1) * num_elements; ++j) {
                    try {
                        ts_om.insert({j, "value_" + std::to_string(j)});
                    } catch (const ordered_map_exception& e) {
                        std::cerr << "Thread " << i << " caught exception: " << e.what() << std::endl;
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
                        if (it != ts_om.end()) {
                            // 查找成功
                        }
                    } catch (const ordered_map_exception& e) {
                        std::cerr << "Thread caught exception during find: " << e.what() << std::endl;
                    }
                }
            });
        }
        
        // 等待所有线程完成
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "Concurrent operations completed successfully" << std::endl;
        std::cout << "Final size: " << ts_om.size() << std::endl;
        
    } catch (const ordered_map_exception& e) {
        std::cout << "Caught ordered_map_exception: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught std::exception: " << e.what() << std::endl;
    }
    
    std::cout << std::endl;
}

/**
 * 示例：内存管理增强
 */
void memory_management_example() {
    std::cout << "=== Memory Management Example ===\n";
    
    try {
        // 创建一个带有内存管理的ordered_map
        ordered_map_with_exceptions<int, std::vector<char>> om;
        
        // 设置较小的内存限制（10KB）
        om.set_memory_limit(10 * 1024);
        
        // 设置内存碎片率阈值为15%
        om.set_fragmentation_threshold(0.15f);
        
        std::cout << "Memory limit set to: " << om.memory_limit() << " bytes" << std::endl;
        
        // 尝试插入大量数据，触发内存限制
        const int element_size = 1024; // 每个元素约1KB
        int inserted_count = 0;
        
        try {
            for (int i = 0; i < 20; ++i) { // 尝试插入20个元素（约20KB）
                std::vector<char> data(element_size, 'a' + (i % 26));
                om.insert({i, std::move(data)});
                inserted_count++;
                std::cout << "Inserted element " << i << ", current memory usage: " 
                          << om.current_memory_usage() << " bytes" << std::endl;
            }
        } catch (const memory_limit_exception& e) {
            std::cout << "Caught expected memory_limit_exception: " << e.what() << std::endl;
            std::cout << "Inserted " << inserted_count << " elements before memory limit reached" << std::endl;
        }
        
        std::cout << "Current size after memory limit: " << om.size() << std::endl;
        
        // 手动触发内存整理
        std::cout << "Triggering memory defragmentation..." << std::endl;
        om.defragment_memory();
        std::cout << "Memory usage after defragmentation: " << om.current_memory_usage() << " bytes" << std::endl;
        
    } catch (const ordered_map_exception& e) {
        std::cout << "Caught ordered_map_exception: " << e.what() << std::endl;
        std::cout << "Exception details: " << e.to_json() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught std::exception: " << e.what() << std::endl;
    }
    
    std::cout << std::endl;
}

/**
 * 示例：安全序列化/反序列化
 */
void safe_serialization_example() {
    std::cout << "=== Safe Serialization Example ===\n";
    
    try {
        // 创建并填充一个ordered_map
        ordered_map_with_exceptions<std::string, int> om;
        om.insert({"one", 1});
        om.insert({"two", 2});
        om.insert({"three", 3});
        
        std::cout << "Original map size: " << om.size() << std::endl;
        
        // 使用字符串流进行序列化
        std::stringstream ss;
        
        // 设置超时为5秒，重试策略为3次重试，每次间隔1秒
        detail_ordered_hash::network_retry_strategy retry_strategy(3, 1000ms);
        
        // 安全序列化
        om.serialize_safe(ss, 5000ms, retry_strategy);
        std::cout << "Serialization completed successfully" << std::endl;
        
        // 创建一个新的ordered_map并反序列化
        ordered_map_with_exceptions<std::string, int> om2;
        ss.seekg(0); // 重置流位置
        
        // 安全反序列化
        om2.deserialize_safe(ss, 5000ms, retry_strategy);
        std::cout << "Deserialization completed successfully" << std::endl;
        
        std::cout << "Deserialized map size: " << om2.size() << std::endl;
        std::cout << "one: " << om2.at("one") << std::endl;
        std::cout << "two: " << om2.at("two") << std::endl;
        std::cout << "three: " << om2.at("three") << std::endl;
        
    } catch (const network_exception& e) {
        std::cout << "Caught network_exception: " << e.what() << std::endl;
        std::cout << "Exception details: " << e.to_json() << std::endl;
    } catch (const ordered_map_exception& e) {
        std::cout << "Caught ordered_map_exception: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught std::exception: " << e.what() << std::endl;
    }
    
    std::cout << std::endl;
}

/**
 * 示例：线程安全迭代器
 */
void thread_safe_iterator_example() {
    std::cout << "=== Thread Safe Iterator Example ===\n";
    
    try {
        // 创建一个线程安全的ordered_map
        thread_safe_ordered_map<int, std::string> ts_om;
        
        // 插入一些元素
        for (int i = 0; i < 10; ++i) {
            ts_om.insert({i, "value_" + std::to_string(i)});
        }
        
        std::cout << "Map size: " << ts_om.size() << std::endl;
        
        // 使用线程安全迭代器遍历
        std::cout << "Elements in map: ";
        for (auto it = ts_om.thread_safe_begin(); it != ts_om.thread_safe_end(); ++it) {
            std::cout << it->first << ":" << it->second << " ";
        }
        std::cout << std::endl;
        
        // 在另一个线程中同时进行修改
        std::thread modifier([&ts_om]() {
            std::this_thread::sleep_for(100ms);
            ts_om.insert({10, "value_10"});
            std::cout << "\nModified map in another thread: inserted 10:value_10" << std::endl;
        });
        
        // 主线程继续使用线程安全迭代器遍历
        std::cout << "\nMain thread iterating again: ";
        for (auto it = ts_om.thread_safe_begin(); it != ts_om.thread_safe_end(); ++it) {
            std::cout << it->first << ":" << it->second << " ";
            std::this_thread::sleep_for(50ms); // 增加并发冲突的可能性
        }
        std::cout << std::endl;
        
        modifier.join();
        
        std::cout << "Final map size: " << ts_om.size() << std::endl;
        
    } catch (const ordered_map_exception& e) {
        std::cout << "Caught ordered_map_exception: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught std::exception: " << e.what() << std::endl;
    }
    
    std::cout << std::endl;
}

/**
 * 主函数
 */
int main() {
    std::cout << "=== tsl::ordered_map Exception Safety Examples ===\n\n";
    
    // 运行所有示例
    basic_usage_example();
    null_pointer_protection_example();
    concurrent_safety_example();
    memory_management_example();
    safe_serialization_example();
    thread_safe_iterator_example();
    
    std::cout << "=== All Examples Completed ===\n";
    
    return 0;
}
