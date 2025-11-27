#ifndef TSL_ORDERED_MAP_NETWORK_H
#define TSL_ORDERED_MAP_NETWORK_H

#include <cstddef>
#include <chrono>
#include <thread>
#include <system_error>
#include <vector>
#include <cstdint>
#include "ordered_map_exceptions.h"

namespace tsl {

namespace detail_ordered_hash {

/**
 * CRC32 校验器
 * 用于验证数据完整性
 */
class crc32_checksum {
public:
    /**
     * 计算数据的CRC32校验和
     */
    static uint32_t calculate(const void* data, size_t length) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        uint32_t crc = 0xFFFFFFFF;
        
        for (size_t i = 0; i < length; ++i) {
            crc ^= bytes[i];
            for (int j = 0; j < 8; ++j) {
                crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
            }
        }
        
        return ~crc;
    }
    
    /**
     * 计算数据的CRC32校验和（模板版本）
     */
    template <class T>
    static uint32_t calculate(const T& data) {
        return calculate(&data, sizeof(T));
    }
    
    /**
     * 验证数据的CRC32校验和
     */
    static bool verify(const void* data, size_t length, uint32_t expected_crc) {
        return calculate(data, length) == expected_crc;
    }
};

/**
 * 超时检测包装器
 * 用于为操作添加超时功能
 */
template <class Func>
auto with_timeout(Func&& func, std::chrono::milliseconds timeout) 
    -> decltype(func()) {
    using result_type = decltype(func());
    
    // 使用线程执行函数
    std::thread worker_thread;
    result_type result;
    std::exception_ptr exception_ptr;
    
    { 
        std::promise<result_type> promise;
        auto future = promise.get_future();
        
        worker_thread = std::thread([&promise, &func]() {
            try {
                promise.set_value(func());
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        });
        
        // 等待结果或超时
        if (future.wait_for(timeout) != std::future_status::ready) {
            // 超时，终止线程并抛出异常
            worker_thread.detach(); // 注意：在某些平台上detach可能不是最佳选择
            TSL_OM_THROW(network_timeout_exception, 
                "Operation timed out after " + std::to_string(timeout.count()) + "ms");
        }
        
        // 获取结果或重新抛出异常
        try {
            result = future.get();
        } catch (...) {
            exception_ptr = std::current_exception();
        }
    }
    
    // 确保线程已结束
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
    
    // 如果有异常，重新抛出
    if (exception_ptr) {
        std::rethrow_exception(exception_ptr);
    }
    
    return result;
}

/**
 * 重试逻辑包装器
 * 用于为操作添加重试功能
 */
template <class Func, class Predicate = std::function<bool(const std::exception&)>>
auto with_retry(Func&& func, int max_retries = 3, 
                std::chrono::milliseconds initial_delay = std::chrono::milliseconds(1000),
                Predicate&& should_retry = [](const std::exception&) { return true; })
    -> decltype(func()) {
    using result_type = decltype(func());
    
    int retry_count = 0;
    std::chrono::milliseconds current_delay = initial_delay;
    
    while (true) {
        try {
            return func();
        } catch (const std::exception& e) {
            if (retry_count >= max_retries || !should_retry(e)) {
                throw;
            }
            
            // 等待后重试
            std::this_thread::sleep_for(current_delay);
            
            // 指数退避（可选）
            current_delay *= 2;
            retry_count++;
        }
    }
}

/**
 * 网络异常处理策略
 * 用于决定是否应该重试操作
 */
class network_retry_strategy {
public:
    explicit network_retry_strategy(int max_retries = 3, 
                                   std::chrono::milliseconds initial_delay = std::chrono::milliseconds(1000))
        : m_max_retries(max_retries), m_initial_delay(initial_delay) {}
    
    /**
     * 检查是否应该重试操作
     */
    bool should_retry(const std::exception& e, int retry_count) const {
        if (retry_count >= m_max_retries) {
            return false;
        }
        
        // 检查异常类型是否是可重试的网络异常
        if (dynamic_cast<const network_io_exception*>(&e) != nullptr ||
            dynamic_cast<const network_timeout_exception*>(&e) != nullptr) {
            return true;
        }
        
        // 检查标准系统错误是否是可重试的
        if (const std::system_error* se = dynamic_cast<const std::system_error*>(&e)) {
            const auto ec = se->code();
            switch (ec.value()) {
                case ECONNRESET: // 连接重置
                case ECONNABORTED: // 连接中止
                case EINTR: // 中断系统调用
                case EAGAIN: // 资源暂时不可用
                case ETIMEDOUT: // 连接超时
                    return true;
                default:
                    return false;
            }
        }
        
        return false;
    }
    
    /**
     * 获取重试延迟
     */
    std::chrono::milliseconds get_retry_delay(int retry_count) const {
        // 简单的递增延迟策略
        return m_initial_delay * (retry_count + 1);
    }
    
    /**
     * 获取最大重试次数
     */
    int max_retries() const noexcept {
        return m_max_retries;
    }
    
    /**
     * 设置最大重试次数
     */
    void set_max_retries(int max_retries) noexcept {
        m_max_retries = max_retries;
    }
    
    /**
     * 设置初始重试延迟
     */
    void set_initial_delay(std::chrono::milliseconds delay) noexcept {
        m_initial_delay = delay;
    }

private:
    int m_max_retries;
    std::chrono::milliseconds m_initial_delay;
};

/**
 * 安全序列化器
 * 为序列化操作添加超时、重试和数据完整性校验
 */
template <class Serializer>
class safe_serializer {
public:
    explicit safe_serializer(Serializer&& serializer, 
                            std::chrono::milliseconds timeout = std::chrono::milliseconds(30000),
                            const network_retry_strategy& retry_strategy = network_retry_strategy())
        : m_serializer(std::forward<Serializer>(serializer)), 
          m_timeout(timeout), m_retry_strategy(retry_strategy) {}
    
    /**
     * 序列化数据（带超时和重试）
     */
    template <class T>
    void serialize(const T& data) {
        // 使用重试包装器
        with_retry(
            [this, &data]() {
                // 使用超时包装器
                with_timeout(
                    [this, &data]() {
                        // 序列化数据
                        m_serializer(data);
                        return true;
                    }, 
                    m_timeout
                );
                return true;
            },
            m_retry_strategy.max_retries(),
            m_retry_strategy.get_retry_delay(0), // 初始延迟
            [this](const std::exception& e) {
                return m_retry_strategy.should_retry(e, 0);
            }
        );
    }
    
    /**
     * 序列化数据并添加CRC校验（带超时和重试）
     */
    template <class T>
    void serialize_with_crc(const T& data) {
        // 序列化数据到临时缓冲区
        std::vector<uint8_t> buffer;
        serialize_to_buffer(data, buffer);
        
        // 计算CRC校验和
        const uint32_t crc = crc32_checksum::calculate(buffer.data(), buffer.size());
        
        // 序列化CRC和数据
        with_retry(
            [this, &buffer, crc]() {
                with_timeout(
                    [this, &buffer, crc]() {
                        m_serializer(crc);
                        m_serializer(buffer.data(), buffer.size());
                        return true;
                    }, 
                    m_timeout
                );
                return true;
            },
            m_retry_strategy.max_retries(),
            m_retry_strategy.get_retry_delay(0),
            [this](const std::exception& e) {
                return m_retry_strategy.should_retry(e, 0);
            }
        );
    }
    
    /**
     * 获取底层序列化器
     */
    Serializer& underlying_serializer() noexcept {
        return m_serializer;
    }
    
    /**
     * 设置超时时间
     */
    void set_timeout(std::chrono::milliseconds timeout) noexcept {
        m_timeout = timeout;
    }
    
    /**
     * 设置重试策略
     */
    void set_retry_strategy(const network_retry_strategy& strategy) {
        m_retry_strategy = strategy;
    }

private:
    Serializer m_serializer;
    std::chrono::milliseconds m_timeout;
    network_retry_strategy m_retry_strategy;
    
    /**
     * 将数据序列化到缓冲区
     */
    template <class T>
    void serialize_to_buffer(const T& data, std::vector<uint8_t>& buffer) {
        // 这里需要根据具体的Serializer类型实现
        // 假设Serializer支持序列化到缓冲区
        // 实际实现可能需要调整
        buffer.resize(sizeof(T));
        m_serializer.serialize_to_buffer(data, buffer.data(), buffer.size());
    }
};

/**
 * 安全反序列化器
 * 为反序列化操作添加超时、重试和数据完整性校验
 */
template <class Deserializer>
class safe_deserializer {
public:
    explicit safe_deserializer(Deserializer&& deserializer, 
                              std::chrono::milliseconds timeout = std::chrono::milliseconds(30000),
                              const network_retry_strategy& retry_strategy = network_retry_strategy())
        : m_deserializer(std::forward<Deserializer>(deserializer)), 
          m_timeout(timeout), m_retry_strategy(retry_strategy) {}
    
    /**
     * 反序列化数据（带超时和重试）
     */
    template <class T>
    T deserialize() {
        return with_retry(
            [this]() {
                return with_timeout(
                    [this]() {
                        return m_deserializer.template operator()<T>();
                    }, 
                    m_timeout
                );
            },
            m_retry_strategy.max_retries(),
            m_retry_strategy.get_retry_delay(0),
            [this](const std::exception& e) {
                return m_retry_strategy.should_retry(e, 0);
            }
        );
    }
    
    /**
     * 反序列化数据并验证CRC校验（带超时和重试）
     */
    template <class T>
    T deserialize_with_crc() {
        return with_retry(
            [this]() {
                return with_timeout(
                    [this]() {
                        // 读取CRC校验和
                        const uint32_t expected_crc = m_deserializer.template operator()<uint32_t>();
                        
                        // 读取数据
                        std::vector<uint8_t> buffer;
                        m_deserializer.read(buffer);
                        
                        // 验证CRC
                        if (!crc32_checksum::verify(buffer.data(), buffer.size(), expected_crc)) {
                            TSL_OM_THROW(data_integrity_exception, 
                                "Data integrity check failed: CRC32 mismatch");
                        }
                        
                        // 反序列化数据
                        return deserialize_from_buffer<T>(buffer);
                    }, 
                    m_timeout
                );
            },
            m_retry_strategy.max_retries(),
            m_retry_strategy.get_retry_delay(0),
            [this](const std::exception& e) {
                return m_retry_strategy.should_retry(e, 0);
            }
        );
    }
    
    /**
     * 获取底层反序列化器
     */
    Deserializer& underlying_deserializer() noexcept {
        return m_deserializer;
    }
    
    /**
     * 设置超时时间
     */
    void set_timeout(std::chrono::milliseconds timeout) noexcept {
        m_timeout = timeout;
    }
    
    /**
     * 设置重试策略
     */
    void set_retry_strategy(const network_retry_strategy& strategy) {
        m_retry_strategy = strategy;
    }

private:
    Deserializer m_deserializer;
    std::chrono::milliseconds m_timeout;
    network_retry_strategy m_retry_strategy;
    
    /**
     * 从缓冲区反序列化数据
     */
    template <class T>
    T deserialize_from_buffer(const std::vector<uint8_t>& buffer) {
        // 这里需要根据具体的Deserializer类型实现
        // 假设Deserializer支持从缓冲区反序列化
        // 实际实现可能需要调整
        if (buffer.size() != sizeof(T)) {
            TSL_OM_THROW(data_integrity_exception, 
                "Data integrity check failed: Invalid buffer size");
        }
        
        T data;
        m_deserializer.deserialize_from_buffer(&data, buffer.data(), buffer.size());
        return data;
    }
};

/**
 * 网络IO异常包装器
 * 用于将系统错误转换为自定义网络异常
 */
template <class Func>
auto wrap_network_io(Func&& func) -> decltype(func()) {
    try {
        return func();
    } catch (const std::system_error& se) {
        const auto ec = se.code();
        std::string message = "Network IO error: " + se.what();
        
        switch (ec.value()) {
            case ECONNRESET:
                message = "Connection reset by peer";
                break;
            case ECONNABORTED:
                message = "Connection aborted";
                break;
            case EADDRNOTAVAIL:
                message = "Address not available";
                break;
            case ENETUNREACH:
                message = "Network unreachable";
                break;
            case ETIMEDOUT:
                message = "Connection timed out";
                break;
            case ECONNREFUSED:
                message = "Connection refused";
                break;
        }
        
        TSL_OM_THROW(network_io_exception, message);
    }
}

} // namespace detail_ordered_hash

} // namespace tsl

#endif // TSL_ORDERED_MAP_NETWORK_H
