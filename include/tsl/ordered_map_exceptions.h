#ifndef TSL_ORDERED_MAP_EXCEPTIONS_H
#define TSL_ORDERED_MAP_EXCEPTIONS_H

#include <exception>
#include <string>
#include <chrono>
#include <sstream>
#include <json/json.h> // 需要JSON库支持，或者使用轻量级JSON实现

// 异常宏开关
#ifndef TSL_ORDERED_MAP_ENABLE_EXCEPTIONS
#define TSL_ORDERED_MAP_ENABLE_EXCEPTIONS 1
#endif

// 线程安全注解宏（用于文档和静态分析）
#define TSL_THREAD_SAFE [[gnu::thread_safe]]
#define TSL_GUARDED_BY(mutex) [[gnu::guarded_by(mutex)]]
#define TSL_REQUIRES(mutex) [[gnu::requires(mutex)]]
#define TSL_ACQUIRE(mutex) [[gnu::acquire_capability(mutex)]]
#define TSL_RELEASE(mutex) [[gnu::release_capability(mutex)]]

namespace tsl {

/**
 * 基础异常类，包含异常发生的位置、时间戳和容器状态
 */
class ordered_map_exception : public std::exception {
public:
    ordered_map_exception(const std::string& message, const char* file, int line)
        : m_message(message), m_file(file), m_line(line),
          m_timestamp(std::chrono::system_clock::now()) {
        format_message();
    }

    const char* what() const noexcept override {
        return m_formatted_message.c_str();
    }

    /**
     * 将异常信息序列化为JSON格式
     */
    std::string to_json() const {
        Json::Value root;
        root["message"] = m_message;
        root["file"] = m_file;
        root["line"] = m_line;
        root["timestamp"] = timestamp_to_string();
        root["type"] = type();
        
        return serialize_json(root);
    }

    virtual const char* type() const noexcept {
        return "ordered_map_exception";
    }

protected:
    std::string m_message;
    const char* m_file;
    int m_line;
    std::chrono::system_clock::time_point m_timestamp;
    std::string m_formatted_message;

    /**
     * 格式化异常信息
     */
    void format_message() {
        std::ostringstream oss;
        oss << "[" << type() << "] " << m_message
            << " (" << m_file << ":" << m_line << ")"
            << " @ " << timestamp_to_string();
        m_formatted_message = oss.str();
    }

    /**
     * 将时间戳转换为字符串
     */
    std::string timestamp_to_string() const {
        auto time_t_val = std::chrono::system_clock::to_time_t(m_timestamp);
        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t_val));
        return buffer;
    }

    /**
     * 序列化JSON对象为字符串
     */
    std::string serialize_json(const Json::Value& root) const {
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        return Json::writeString(writer, root);
    }
};

/**
 * 空指针异常
 */
class null_pointer_exception : public ordered_map_exception {
public:
    null_pointer_exception(const std::string& message, const char* file, int line)
        : ordered_map_exception(message, file, line) {}

    const char* type() const noexcept override {
        return "null_pointer_exception";
    }
};

/**
 * 网络超时异常
 */
class network_timeout_exception : public ordered_map_exception {
public:
    network_timeout_exception(const std::string& message, const char* file, int line)
        : ordered_map_exception(message, file, line) {}

    const char* type() const noexcept override {
        return "network_timeout_exception";
    }
};

/**
 * 网络IO异常
 */
class network_io_exception : public ordered_map_exception {
public:
    network_io_exception(const std::string& message, const char* file, int line)
        : ordered_map_exception(message, file, line) {}

    const char* type() const noexcept override {
        return "network_io_exception";
    }
};

/**
 * 数据完整性异常（CRC校验失败）
 */
class data_integrity_exception : public ordered_map_exception {
public:
    data_integrity_exception(const std::string& message, const char* file, int line)
        : ordered_map_exception(message, file, line) {}

    const char* type() const noexcept override {
        return "data_integrity_exception";
    }
};

/**
 * 内存限制异常
 */
class memory_limit_exception : public ordered_map_exception {
public:
    memory_limit_exception(const std::string& message, const char* file, int line)
        : ordered_map_exception(message, file, line) {}

    const char* type() const noexcept override {
        return "memory_limit_exception";
    }
};

/**
 * 内存分配异常
 */
class memory_allocation_exception : public ordered_map_exception {
public:
    memory_allocation_exception(const std::string& message, const char* file, int line)
        : ordered_map_exception(message, file, line) {}

    const char* type() const noexcept override {
        return "memory_allocation_exception";
    }
};

/**
 * 迭代器失效异常
 */
class iterator_invalid_exception : public ordered_map_exception {
public:
    iterator_invalid_exception(const std::string& message, const char* file, int line)
        : ordered_map_exception(message, file, line) {}

    const char* type() const noexcept override {
        return "iterator_invalid_exception";
    }
};

/**
 * 未初始化函数对象异常
 */
class uninitialized_function_exception : public ordered_map_exception {
public:
    uninitialized_function_exception(const std::string& message, const char* file, int line)
        : ordered_map_exception(message, file, line) {}

    const char* type() const noexcept override {
        return "uninitialized_function_exception";
    }
};

/**
 * 异常抛出宏定义
 */
#if TSL_ORDERED_MAP_ENABLE_EXCEPTIONS
#define TSL_OM_THROW(ex_type, message) \
    throw ex_type(message, __FILE__, __LINE__)
#else
#define TSL_OM_THROW(ex_type, message) \
    do { \
        std::cerr << "[" << #ex_type << "] " << message \
                  << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        std::terminate(); \
    } while (0)
#endif

/**
 * 空指针检查宏
 */
#define TSL_OM_CHECK_NULL(ptr, message) \
    do { \
        if (ptr == nullptr) { \
            TSL_OM_THROW(null_pointer_exception, message); \
        } \
    } while (0)

/**
 * 函数对象初始化检查宏
 */
#define TSL_OM_CHECK_FUNCTION_INIT(func, message) \
    do { \
        if (!func) { \
            TSL_OM_THROW(uninitialized_function_exception, message); \
        } \
    } while (0)

/**
 * 迭代器有效性检查宏
 */
#define TSL_OM_CHECK_ITERATOR_VALID(iter, container, message) \
    do { \
        if (!container.is_iterator_valid(iter)) { \
            TSL_OM_THROW(iterator_invalid_exception, message); \
        } \
    } while (0)

} // namespace tsl

#endif // TSL_ORDERED_MAP_EXCEPTIONS_H
