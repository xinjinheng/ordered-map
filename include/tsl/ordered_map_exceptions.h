#ifndef TSL_ORDERED_MAP_EXCEPTIONS_H
#define TSL_ORDERED_MAP_EXCEPTIONS_H

#include <exception>
#include <string>
#include <chrono>
#include <sstream>

namespace tsl {

/**
 * Base class for all ordered_map exceptions
 */
class ordered_map_exception : public std::exception {
public:
    ordered_map_exception(const std::string& message, const std::string& file, int line)
        : m_message(message), m_file(file), m_line(line), m_timestamp(std::chrono::system_clock::now()) {
        std::ostringstream oss;
        oss << "[" << m_file << ":" << m_line << "] " << m_message;
        m_what = oss.str();
    }

    virtual ~ordered_map_exception() noexcept = default;

    const char* what() const noexcept override {
        return m_what.c_str();
    }

    const std::string& message() const noexcept {
        return m_message;
    }

    const std::string& file() const noexcept {
        return m_file;
    }

    int line() const noexcept {
        return m_line;
    }

    const std::chrono::system_clock::time_point& timestamp() const noexcept {
        return m_timestamp;
    }

    virtual std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"type\":\"ordered_map_exception\",";
        oss << "\"message\":\"" << m_message << "\",";
        oss << "\"file\":\"" << m_file << "\",";
        oss << "\"line\":" << m_line << ",";
        oss << "\"timestamp\":" << std::chrono::system_clock::to_time_t(m_timestamp);
        oss << "}";
        return oss.str();
    }

private:
    std::string m_message;
    std::string m_file;
    int m_line;
    std::chrono::system_clock::time_point m_timestamp;
    std::string m_what;
};

/**
 * Exception thrown when a null pointer is encountered
 */
class null_pointer_exception : public ordered_map_exception {
public:
    null_pointer_exception(const std::string& message, const std::string& file, int line)
        : ordered_map_exception(message, file, line) {}

    std::string to_json() const override {
        std::ostringstream oss;
        oss << "{";
        oss << "\"type\":\"null_pointer_exception\",";
        oss << "\"message\":\"" << message() << "\",";
        oss << "\"file\":\"" << file() << "\",";
        oss << "\"line\":" << line() << ",";
        oss << "\"timestamp\":" << std::chrono::system_clock::to_time_t(timestamp());
        oss << "}";
        return oss.str();
    }
};

/**
 * Exception thrown when a network timeout occurs
 */
class network_timeout_exception : public ordered_map_exception {
public:
    network_timeout_exception(const std::string& message, const std::string& file, int line)
        : ordered_map_exception(message, file, line) {}

    std::string to_json() const override {
        std::ostringstream oss;
        oss << "{";
        oss << "\"type\":\"network_timeout_exception\",";
        oss << "\"message\":\"" << message() << "\",";
        oss << "\"file\":\"" << file() << "\",";
        oss << "\"line\":" << line() << ",";
        oss << "\"timestamp\":" << std::chrono::system_clock::to_time_t(timestamp());
        oss << "}";
        return oss.str();
    }
};

/**
 * Exception thrown when the memory limit is exceeded
 */
class memory_limit_exception : public ordered_map_exception {
public:
    memory_limit_exception(const std::string& message, const std::string& file, int line)
        : ordered_map_exception(message, file, line) {}

    std::string to_json() const override {
        std::ostringstream oss;
        oss << "{";
        oss << "\"type\":\"memory_limit_exception\",";
        oss << "\"message\":\"" << message() << "\",";
        oss << "\"file\":\"" << file() << "\",";
        oss << "\"line\":" << line() << ",";
        oss << "\"timestamp\":" << std::chrono::system_clock::to_time_t(timestamp());
        oss << "}";
        return oss.str();
    }
};

/**
 * Exception thrown when a network error occurs
 */
class network_exception : public ordered_map_exception {
public:
    network_exception(const std::string& message, const std::string& file, int line)
        : ordered_map_exception(message, file, line) {}

    std::string to_json() const override {
        std::ostringstream oss;
        oss << "{";
        oss << "\"type\":\"network_exception\",";
        oss << "\"message\":\"" << message() << "\",";
        oss << "\"file\":\"" << file() << "\",";
        oss << "\"line\":" << line() << ",";
        oss << "\"timestamp\":" << std::chrono::system_clock::to_time_t(timestamp());
        oss << "}";
        return oss.str();
    }
};

/**
 * Exception thrown when data integrity check fails
 */
class data_integrity_exception : public ordered_map_exception {
public:
    data_integrity_exception(const std::string& message, const std::string& file, int line)
        : ordered_map_exception(message, file, line) {}

    std::string to_json() const override {
        std::ostringstream oss;
        oss << "{";
        oss << "\"type\":\"data_integrity_exception\",";
        oss << "\"message\":\"" << message() << "\",";
        oss << "\"file\":\"" << file() << "\",";
        oss << "\"line\":" << line() << ",";
        oss << "\"timestamp\":" << std::chrono::system_clock::to_time_t(timestamp());
        oss << "}";
        return oss.str();
    }
};

/**
 * Exception thrown when an invalid iterator is used
 */
class invalid_iterator_exception : public ordered_map_exception {
public:
    invalid_iterator_exception(const std::string& message, const std::string& file, int line)
        : ordered_map_exception(message, file, line) {}

    std::string to_json() const override {
        std::ostringstream oss;
        oss << "{";
        oss << "\"type\":\"invalid_iterator_exception\",";
        oss << "\"message\":\"" << message() << "\",";
        oss << "\"file\":\"" << file() << "\",";
        oss << "\"line\":" << line() << ",";
        oss << "\"timestamp\":" << std::chrono::system_clock::to_time_t(timestamp());
        oss << "}";
        return oss.str();
    }
};

/**
 * Exception thrown when a function object is in an invalid state
 */
class invalid_function_object_exception : public ordered_map_exception {
public:
    invalid_function_object_exception(const std::string& message, const std::string& file, int line)
        : ordered_map_exception(message, file, line) {}

    std::string to_json() const override {
        std::ostringstream oss;
        oss << "{";
        oss << "\"type\":\"invalid_function_object_exception\",";
        oss << "\"message\":\"" << message() << "\",";
        oss << "\"file\":\"" << file() << "\",";
        oss << "\"line\":" << line() << ",";
        oss << "\"timestamp\":" << std::chrono::system_clock::to_time_t(timestamp());
        oss << "}";
        return oss.str();
    }
};

} // end namespace tsl

#endif // TSL_ORDERED_MAP_EXCEPTIONS_H
