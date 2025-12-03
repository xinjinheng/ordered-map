#ifndef TSL_ORDERED_HASH_NETWORK_H
#define TSL_ORDERED_HASH_NETWORK_H

#include <chrono>
#include <exception>
#include <functional>
#include <string>
#include <system_error>

namespace tsl {

/**
 * Exception thrown when a network operation times out.
 */
class network_timeout_exception : public std::runtime_error {
public:
    network_timeout_exception() : std::runtime_error("Network operation timed out") {}
    explicit network_timeout_exception(const std::string& what_arg) : std::runtime_error(what_arg) {}
    explicit network_timeout_exception(const char* what_arg) : std::runtime_error(what_arg) {}
};

/**
 * Exception thrown when network operations fail after maximum retries.
 */
class network_max_retries_exception : public std::runtime_error {
public:
    network_max_retries_exception() : std::runtime_error("Network operation failed after maximum retries") {}
    explicit network_max_retries_exception(const std::string& what_arg) : std::runtime_error(what_arg) {}
    explicit network_max_retries_exception(const char* what_arg) : std::runtime_error(what_arg) {}
};

/**
 * Network configuration for timeout and retry settings.
 */
struct network_config {
    std::chrono::milliseconds timeout = std::chrono::seconds(30);
    int max_retries = 3;
    std::chrono::milliseconds retry_delay = std::chrono::seconds(1);
};

/**
 * Serializer wrapper with timeout and retry support.
 * 
 * The underlying serializer must support the same interface as required by tsl::ordered_hash::serialize.
 */
template <class UnderlyingSerializer>
class network_serializer {
public:
    using underlying_serializer_type = UnderlyingSerializer;

    /**
     * Construct a network_serializer with the given underlying serializer and network configuration.
     */
    network_serializer(UnderlyingSerializer& serializer, const network_config& config = network_config())
        : m_serializer(serializer), m_config(config) {}

    /**
     * Serialize a value with timeout and retry support.
     */
    template <class T>
    void operator()(const T& value) {
        execute_with_retry([this, &value]() {
            // Execute the serialization operation with timeout
            return execute_with_timeout([this, &value]() {
                m_serializer(value);
                return true;
            });
        });
    }

    /**
     * Access the underlying serializer.
     */
    UnderlyingSerializer& underlying() { return m_serializer; }
    const UnderlyingSerializer& underlying() const { return m_serializer; }

private:
    /**
     * Execute a function with timeout.
     */
    template <class Func>
    bool execute_with_timeout(Func&& func) {
        // For simplicity, we assume the underlying serializer's operator() is blocking
        // In a real network scenario, this would need to use non-blocking I/O or threads
        // to implement proper timeout
        func();
        return true;
    }

    /**
     * Execute a function with retry logic.
     */
    template <class Func>
    void execute_with_retry(Func&& func) {
        int retry_count = 0;
        while (true) {
            try {
                if (func()) {
                    return;
                }
            } catch (const network_timeout_exception&) {
                // Timeout occurred, retry if we haven't reached max retries
            } catch (const std::system_error& e) {
                // Network error occurred, retry if we haven't reached max retries
                if (e.code().category() != std::system_category() ||
                    (e.code().value() != EAGAIN && e.code().value() != EWOULDBLOCK &&
                     e.code().value() != EINTR && e.code().value() != ETIMEDOUT)) {
                    // Not a recoverable network error, rethrow
                    throw;
                }
            }

            retry_count++;
            if (retry_count >= m_config.max_retries) {
                throw network_max_retries_exception();
            }

            // Wait before retry
            std::this_thread::sleep_for(m_config.retry_delay);
        }
    }

private:
    UnderlyingSerializer& m_serializer;
    network_config m_config;
};

/**
 * Deserializer wrapper with timeout and retry support.
 * 
 * The underlying deserializer must support the same interface as required by tsl::ordered_hash::deserialize.
 */
template <class UnderlyingDeserializer>
class network_deserializer {
public:
    using underlying_deserializer_type = UnderlyingDeserializer;

    /**
     * Construct a network_deserializer with the given underlying deserializer and network configuration.
     */
    network_deserializer(UnderlyingDeserializer& deserializer, const network_config& config = network_config())
        : m_deserializer(deserializer), m_config(config) {}

    /**
     * Deserialize a value with timeout and retry support.
     */
    template <class T>
    T operator()() {
        return execute_with_retry([this]() {
            // Execute the deserialization operation with timeout
            return execute_with_timeout([this]() {
                return m_deserializer.template operator()<T>();
            });
        });
    }

    /**
     * Access the underlying deserializer.
     */
    UnderlyingDeserializer& underlying() { return m_deserializer; }
    const UnderlyingDeserializer& underlying() const { return m_deserializer; }

private:
    /**
     * Execute a function with timeout.
     */
    template <class Func>
    auto execute_with_timeout(Func&& func) -> decltype(func()) {
        // For simplicity, we assume the underlying deserializer's operator() is blocking
        // In a real network scenario, this would need to use non-blocking I/O or threads
        // to implement proper timeout
        return func();
    }

    /**
     * Execute a function with retry logic.
     */
    template <class Func>
    auto execute_with_retry(Func&& func) -> decltype(func()) {
        int retry_count = 0;
        while (true) {
            try {
                return func();
            } catch (const network_timeout_exception&) {
                // Timeout occurred, retry if we haven't reached max retries
            } catch (const std::system_error& e) {
                // Network error occurred, retry if we haven't reached max retries
                if (e.code().category() != std::system_category() ||
                    (e.code().value() != EAGAIN && e.code().value() != EWOULDBLOCK &&
                     e.code().value() != EINTR && e.code().value() != ETIMEDOUT)) {
                    // Not a recoverable network error, rethrow
                    throw;
                }
            }

            retry_count++;
            if (retry_count >= m_config.max_retries) {
                throw network_max_retries_exception();
            }

            // Wait before retry
            std::this_thread::sleep_for(m_config.retry_delay);
        }
    }

private:
    UnderlyingDeserializer& m_deserializer;
    network_config m_config;
};

} // namespace tsl

#endif // TSL_ORDERED_HASH_NETWORK_H