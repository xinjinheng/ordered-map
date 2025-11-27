# tsl::ordered_map 异常安全防护机制

## 概述

本文档介绍了为 tsl::ordered_map 添加的全面运行时异常安全防护机制，增强了在高并发与异常输入场景下的稳定性。

## 功能特性

### 1. 空指针防护体系

- **关键参数空指针检测**：在所有涉及指针操作的成员函数（如 insert、emplace、find 等）中添加空指针检测逻辑
- **可定制组件空状态校验**：为哈希函数、键比较函数等可定制组件添加空状态校验
- **迭代器有效性验证**：在迭代器操作中增加指针有效性验证，防止对已失效迭代器进行解引用操作

### 2. 网络相关异常处理

- **超时检测机制**：在 serialize/deserialize 方法中添加网络超时检测机制，支持设置超时阈值（默认 30 秒）
- **重试逻辑**：实现网络中断后的重试逻辑，支持自定义重试次数（默认 3 次）和重试间隔（默认 1 秒递增）
- **数据完整性校验**：通过 CRC32 校验确保序列化数据在传输过程中未被篡改
- **网络 IO 异常处理**：处理网络 IO 异常（如连接重置、地址不可达等），封装专门的 NetworkException 异常类

### 3. 内存管理增强

- **内存分配异常捕获**：为内存分配操作添加异常捕获，当 allocator 分配内存失败时进行优雅处理
- **内存使用上限控制**：可通过 set_memory_limit 方法设置最大内存占用，超过时触发 LRU 淘汰机制
- **内存碎片检测**：在大规模数据插入场景下添加内存碎片检测，当碎片率超过阈值（默认 20%）时自动进行内存整理

### 4. 并发安全增强

- **线程安全注解**：为所有公共成员函数添加线程安全注解，使用 thread_local 存储线程私有状态
- **读写锁机制**：实现读写锁机制，支持多线程并发读、单线程写的场景
- **线程安全迭代器**：对迭代器进行线程安全封装，防止在迭代过程中其他线程修改容器导致的数据不一致

### 5. 异常信息增强

- **自定义异常体系**：包括 NullPointerException、NetworkTimeoutException、MemoryLimitException 等
- **详细异常信息**：所有异常信息包含发生位置（文件名、行号）、时间戳、容器状态快照（大小、负载因子等）
- **异常信息序列化**：实现异常信息的序列化功能，可将异常详情输出为 JSON 格式便于日志分析

### 6. 兼容性处理

- **异常防护开关**：异常防护机制可通过宏定义（TSL_ORDERED_MAP_ENABLE_EXCEPTIONS）开关
- **编译器版本兼容**：对 C++11 至 C++20 各版本编译器进行兼容处理
- **API 兼容性**：保留原有 API 接口的兼容性，所有新增异常处理逻辑不改变现有接口的函数签名

## 快速开始

### 1. 包含头文件

```cpp
#include "tsl/ordered_map_with_exceptions.h"
#include "tsl/ordered_map_exceptions.h"
```

### 2. 基本使用

```cpp
// 创建一个带有异常防护的ordered_map
ordered_map_with_exceptions<std::string, int> om;

// 设置内存使用上限为1MB
om.set_memory_limit(1024 * 1024);

// 插入元素
om.insert({"apple", 1});
om.insert({"banana", 2});
om.insert({"cherry", 3});

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
```

### 3. 异常处理

```cpp
try {
    // 尝试访问不存在的元素
    std::cout << "grape: " << om.at("grape") << std::endl;
} catch (const out_of_range_exception& e) {
    std::cout << "Caught expected exception: " << e.what() << std::endl;
    std::cout << "Exception details: " << e.to_json() << std::endl;
}
```

## 高级特性

### 1. 并发安全使用

```cpp
// 创建一个线程安全的ordered_map
thread_safe_ordered_map<int, std::string> ts_om;

// 多线程并发插入
std::vector<std::thread> threads;
for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&ts_om, i]() {
        for (int j = i * 100; j < (i + 1) * 100; ++j) {
            ts_om.insert({j, "value_" + std::to_string(j)});
        }
    });
}

// 等待所有线程完成
for (auto& thread : threads) {
    thread.join();
}
```

### 2. 安全序列化/反序列化

```cpp
// 创建并填充一个ordered_map
ordered_map_with_exceptions<std::string, int> om;
om.insert({"one", 1});
om.insert({"two", 2});
om.insert({"three", 3});

// 使用字符串流进行序列化
std::stringstream ss;

// 设置超时为5秒，重试策略为3次重试，每次间隔1秒
detail_ordered_hash::network_retry_strategy retry_strategy(3, 1000ms);

// 安全序列化
om.serialize_safe(ss, 5000ms, retry_strategy);

// 创建一个新的ordered_map并反序列化
ordered_map_with_exceptions<std::string, int> om2;
ss.seekg(0); // 重置流位置

// 安全反序列化
om2.deserialize_safe(ss, 5000ms, retry_strategy);
```

### 3. 线程安全迭代器

```cpp
// 使用线程安全迭代器遍历
for (auto it = ts_om.thread_safe_begin(); it != ts_om.thread_safe_end(); ++it) {
    std::cout << it->first << ":" << it->second << " ";
}
```

### 4. 内存管理

```cpp
// 设置内存使用上限
om.set_memory_limit(10 * 1024 * 1024); // 10MB

// 设置内存碎片率阈值
om.set_fragmentation_threshold(0.15f); // 15%

// 手动触发内存整理
om.defragment_memory();

// 获取当前内存使用情况
std::cout << "Current memory usage: " << om.current_memory_usage() << " bytes" << std::endl;
```

## 异常体系

### 基础异常类

- **ordered_map_exception**：所有自定义异常的基类

### 派生异常类

- **null_pointer_exception**：空指针异常
- **network_exception**：网络相关异常
  - **network_timeout_exception**：网络超时异常
  - **network_connection_exception**：网络连接异常
  - **network_corruption_exception**：数据损坏异常
- **memory_exception**：内存相关异常
  - **memory_allocation_exception**：内存分配异常
  - **memory_limit_exception**：内存限制异常
- **invalid_iterator_exception**：无效迭代器异常
- **out_of_range_exception**：范围越界异常
- **uninitialized_function_exception**：函数未初始化异常

### 异常信息访问

```cpp
try {
    // 触发异常
    om.at("non_existent");
} catch (const ordered_map_exception& e) {
    // 基本信息
    std::cout << "Exception: " << e.what() << std::endl;
    std::cout << "Type: " << e.exception_type() << std::endl;
    
    // 位置信息
    std::cout << "File: " << e.file() << std::endl;
    std::cout << "Line: " << e.line() << std::endl;
    
    // 时间戳
    std::cout << "Timestamp: " << e.timestamp() << std::endl;
    
    // 容器状态快照
    if (e.state_snapshot()) {
        std::cout << "Container size: " << e.state_snapshot()->size << std::endl;
        std::cout << "Max size: " << e.state_snapshot()->max_size << std::endl;
    }
    
    // JSON格式输出
    std::cout << "JSON: " << e.to_json() << std::endl;
}
```

## 配置选项

### 宏定义开关

- **TSL_ORDERED_MAP_ENABLE_EXCEPTIONS**：启用/禁用异常防护机制（默认启用）
- **TSL_ORDERED_MAP_ENABLE_THREAD_SAFETY**：启用/禁用线程安全特性（默认启用）
- **TSL_ORDERED_MAP_ENABLE_MEMORY_MANAGEMENT**：启用/禁用内存管理增强（默认启用）
- **TSL_ORDERED_MAP_ENABLE_NETWORK_SAFETY**：启用/禁用网络安全特性（默认启用）

### 编译时配置

```cpp
// 禁用异常防护
#define TSL_ORDERED_MAP_ENABLE_EXCEPTIONS 0
#include "tsl/ordered_map_with_exceptions.h"

// 仅启用基本异常防护，禁用线程安全
#define TSL_ORDERED_MAP_ENABLE_THREAD_SAFETY 0
#include "tsl/ordered_map_with_exceptions.h"
```

## 性能考虑

### 异常防护的性能开销

- **空指针检查**：几乎没有性能开销
- **迭代器有效性检查**：O(1) 时间复杂度
- **内存管理**：定期检查内存使用情况，开销很小
- **并发安全**：读写锁会带来一定的性能开销，但对于读多写少的场景影响很小
- **异常信息收集**：仅在异常发生时收集，正常情况下没有开销

### 性能优化建议

1. **根据场景选择锁策略**：
   - 读多写少：使用默认的读写锁策略
   - 单线程环境：使用 no_lock_policy
   - 写多读少：使用 exclusive_lock_policy

2. **合理设置内存参数**：
   - 根据实际可用内存设置内存上限
   - 根据应用特性调整内存碎片率阈值

3. **异常处理策略**：
   - 仅在必要时捕获异常
   - 避免在性能关键路径上抛出异常

## 测试

### 单元测试

提供了全面的单元测试覆盖：

- 空指针注入测试
- 网络异常模拟测试
- 内存限制测试
- 并发访问测试
- 迭代器有效性测试
- 异常信息测试

### 压力测试

- 高并发场景下的性能测试
- 内存极限情况下的稳定性测试
- 异常频繁发生时的性能影响测试

### 内存泄漏检测

- 异常场景下的资源释放测试
- 长时间运行的内存泄漏检测

## 示例代码

完整的示例代码可以在 `example/ordered_map_example.cpp` 中找到，展示了所有主要功能的使用方法。

## 构建和安装

### 构建要求

- C++11 或更高版本
- CMake 3.10 或更高版本
- 支持的编译器：GCC 4.8+, Clang 3.4+, MSVC 2015+

### 构建步骤

```bash
mkdir build
cd build
cmake ..
make
```

### 运行示例

```bash
./example/ordered_map_example
```

### 运行测试

```bash
./test/ordered_map_exception_tests
```

## 许可证

与 tsl::ordered_map 相同，采用 MIT 许可证。

## 贡献

欢迎提交 Issue 和 Pull Request 来改进这个异常安全防护机制。

## 联系方式

如有问题或建议，请通过 GitHub Issue 联系。
