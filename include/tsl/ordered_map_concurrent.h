#ifndef TSL_ORDERED_MAP_CONCURRENT_H
#define TSL_ORDERED_MAP_CONCURRENT_H

#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <type_traits>
#include "ordered_map_exceptions.h"

namespace tsl {

namespace detail_ordered_hash {

/**
 * 线程安全注解宏定义
 * 用于标记函数的线程安全属性
 */
#ifdef TSL_ORDERED_MAP_ENABLE_THREAD_SAFETY_ANNOTATIONS
    #define TSL_OM_THREAD_SAFE [[gnu::annotate("thread_safe")]]
    #define TSL_OM_GUARDED_BY(mutex) [[gnu::annotate("guarded_by(" #mutex ")")]]
    #define TSL_OM_ACQUIRED_BEFORE(...) [[gnu::annotate("acquired_before(" #__VA_ARGS__ ")")]]
    #define TSL_OM_ACQUIRED_AFTER(...) [[gnu::annotate("acquired_after(" #__VA_ARGS__ ")")]]
    #define TSL_OM_REQUIRES(...) [[gnu::annotate("requires_capability(" #__VA_ARGS__ ")")]]
    #define TSL_OM_RELEASES(...) [[gnu::annotate("releases_capability(" #__VA_ARGS__ ")")]]
    #define TSL_OM_ACQUIRES(...) [[gnu::annotate("acquires_capability(" #__VA_ARGS__ ")")]]
    #define TSL_OM_EXCLUDES(...) [[gnu::annotate("locks_excluded(" #__VA_ARGS__ ")")]]
    #define TSL_OM_ASSERT_CAPABILITY(...) [[gnu::annotate("assert_capability(" #__VA_ARGS__ ")")]]
    #define TSL_OM_RETURN_CAPABILITY(...) [[gnu::annotate("return_capability(" #__VA_ARGS__ ")")]]
#else
    #define TSL_OM_THREAD_SAFE
    #define TSL_OM_GUARDED_BY(mutex)
    #define TSL_OM_ACQUIRED_BEFORE(...)
    #define TSL_OM_ACQUIRED_AFTER(...)
    #define TSL_OM_REQUIRES(...)
    #define TSL_OM_RELEASES(...)
    #define TSL_OM_ACQUIRES(...)
    #define TSL_OM_EXCLUDES(...)
    #define TSL_OM_ASSERT_CAPABILITY(...)
    #define TSL_OM_RETURN_CAPABILITY(...)
#endif

/**
 * 读写锁策略
 * 支持多线程并发读、单线程写
 */
class read_write_lock_policy {
public:
    using mutex_type = std::shared_mutex;
    using read_lock = std::shared_lock<mutex_type>;
    using write_lock = std::unique_lock<mutex_type>;
    
    read_write_lock_policy() = default;
    
    // 禁止拷贝和移动
    read_write_lock_policy(const read_write_lock_policy&) = delete;
    read_write_lock_policy& operator=(const read_write_lock_policy&) = delete;
    read_write_lock_policy(read_write_lock_policy&&) = delete;
    read_write_lock_policy& operator=(read_write_lock_policy&&) = delete;
    
    /**
     * 获取读锁
     */
    read_lock acquire_read_lock() const {
        return read_lock(m_mutex);
    }
    
    /**
     * 获取写锁
     */
    write_lock acquire_write_lock() {
        return write_lock(m_mutex);
    }
    
    /**
     * 尝试获取读锁
     */
    bool try_acquire_read_lock() const {
        return m_mutex.try_lock_shared();
    }
    
    /**
     * 尝试获取写锁
     */
    bool try_acquire_write_lock() {
        return m_mutex.try_lock();
    }

private:
    mutable mutex_type m_mutex;
};

/**
 * 独占锁策略
 * 支持单线程读写
 */
class exclusive_lock_policy {
public:
    using mutex_type = std::mutex;
    using lock_type = std::unique_lock<mutex_type>;
    
    exclusive_lock_policy() = default;
    
    // 禁止拷贝和移动
    exclusive_lock_policy(const exclusive_lock_policy&) = delete;
    exclusive_lock_policy& operator=(const exclusive_lock_policy&) = delete;
    exclusive_lock_policy(exclusive_lock_policy&&) = delete;
    exclusive_lock_policy& operator=(exclusive_lock_policy&&) = delete;
    
    /**
     * 获取锁
     */
    lock_type acquire_lock() const {
        return lock_type(m_mutex);
    }
    
    /**
     * 尝试获取锁
     */
    bool try_acquire_lock() const {
        return m_mutex.try_lock();
    }

private:
    mutable mutex_type m_mutex;
};

/**
 * 无锁策略
 * 不提供任何线程安全保证
 */
class no_lock_policy {
public:
    /**
     * 空锁类型
     */
    class null_lock {
    public:
        null_lock() = default;
        explicit null_lock(const no_lock_policy&) {}
        
        void lock() const noexcept {}
        bool try_lock() const noexcept { return true; }
        void unlock() const noexcept {}
    };
    
    using lock_type = null_lock;
    
    no_lock_policy() = default;
    
    // 允许拷贝和移动
    no_lock_policy(const no_lock_policy&) = default;
    no_lock_policy& operator=(const no_lock_policy&) = default;
    no_lock_policy(no_lock_policy&&) = default;
    no_lock_policy& operator=(no_lock_policy&&) = default;
    
    /**
     * 获取空锁
     */
    lock_type acquire_lock() const {
        return lock_type(*this);
    }
    
    /**
     * 尝试获取空锁（总是成功）
     */
    bool try_acquire_lock() const noexcept {
        return true;
    }
};

/**
 * 线程安全迭代器基类
 * 为迭代器添加线程安全保护
 */
template <class Iterator, class LockPolicy>
class thread_safe_iterator {
public:
    using iterator_category = typename Iterator::iterator_category;
    using value_type = typename Iterator::value_type;
    using difference_type = typename Iterator::difference_type;
    using pointer = typename Iterator::pointer;
    using reference = typename Iterator::reference;
    
    /**
     * 构造函数
     */
    thread_safe_iterator(Iterator it, const LockPolicy& lock_policy)
        : m_iterator(std::move(it)), 
          m_lock(lock_policy.acquire_lock()),
          m_valid(true) {}
    
    /**
     * 拷贝构造函数（禁止）
     */
    thread_safe_iterator(const thread_safe_iterator&) = delete;
    
    /**
     * 移动构造函数
     */
    thread_safe_iterator(thread_safe_iterator&& other) noexcept
        : m_iterator(std::move(other.m_iterator)),
          m_lock(std::move(other.m_lock)),
          m_valid(other.m_valid) {
        other.m_valid = false;
    }
    
    /**
     * 拷贝赋值运算符（禁止）
     */
    thread_safe_iterator& operator=(const thread_safe_iterator&) = delete;
    
    /**
     * 移动赋值运算符
     */
    thread_safe_iterator& operator=(thread_safe_iterator&& other) noexcept {
        if (this != &other) {
            m_iterator = std::move(other.m_iterator);
            m_lock = std::move(other.m_lock);
            m_valid = other.m_valid;
            other.m_valid = false;
        }
        return *this;
    }
    
    /**
     * 解引用操作符
     */
    reference operator*() const {
        check_validity();
        return *m_iterator;
    }
    
    /**
     * 成员访问操作符
     */
    pointer operator->() const {
        check_validity();
        return m_iterator.operator->();
    }
    
    /**
     * 前缀递增操作符
     */
    thread_safe_iterator& operator++() {
        check_validity();
        ++m_iterator;
        return *this;
    }
    
    /**
     * 后缀递增操作符
     */
    thread_safe_iterator operator++(int) {
        check_validity();
        auto temp = *this;
        ++m_iterator;
        return temp;
    }
    
    /**
     * 前缀递减操作符
     */
    thread_safe_iterator& operator--() {
        check_validity();
        --m_iterator;
        return *this;
    }
    
    /**
     * 后缀递减操作符
     */
    thread_safe_iterator operator--(int) {
        check_validity();
        auto temp = *this;
        --m_iterator;
        return temp;
    }
    
    /**
     * 相等比较操作符
     */
    bool operator==(const thread_safe_iterator& other) const {
        check_validity();
        other.check_validity();
        return m_iterator == other.m_iterator;
    }
    
    /**
     * 不等比较操作符
     */
    bool operator!=(const thread_safe_iterator& other) const {
        return !(*this == other);
    }
    
    /**
     * 距离操作符
     */
    difference_type operator-(const thread_safe_iterator& other) const {
        check_validity();
        other.check_validity();
        return m_iterator - other.m_iterator;
    }
    
    /**
     * 偏移操作符
     */
    thread_safe_iterator operator+(difference_type n) const {
        check_validity();
        return thread_safe_iterator(m_iterator + n, *m_lock.mutex());
    }
    
    /**
     * 偏移赋值操作符
     */
    thread_safe_iterator& operator+=(difference_type n) {
        check_validity();
        m_iterator += n;
        return *this;
    }
    
    /**
     * 偏移操作符
     */
    thread_safe_iterator operator-(difference_type n) const {
        check_validity();
        return thread_safe_iterator(m_iterator - n, *m_lock.mutex());
    }
    
    /**
     * 偏移赋值操作符
     */
    thread_safe_iterator& operator-=(difference_type n) {
        check_validity();
        m_iterator -= n;
        return *this;
    }
    
    /**
     * 下标操作符
     */
    reference operator[](difference_type n) const {
        check_validity();
        return m_iterator[n];
    }
    
    /**
     * 小于比较操作符
     */
    bool operator<(const thread_safe_iterator& other) const {
        check_validity();
        other.check_validity();
        return m_iterator < other.m_iterator;
    }
    
    /**
     * 大于比较操作符
     */
    bool operator>(const thread_safe_iterator& other) const {
        return other < *this;
    }
    
    /**
     * 小于等于比较操作符
     */
    bool operator<=(const thread_safe_iterator& other) const {
        return !(other < *this);
    }
    
    /**
     * 大于等于比较操作符
     */
    bool operator>=(const thread_safe_iterator& other) const {
        return !(*this < other);
    }
    
    /**
     * 检查迭代器是否有效
     */
    bool is_valid() const noexcept {
        return m_valid;
    }
    
    /**
     * 使迭代器失效
     */
    void invalidate() noexcept {
        m_valid = false;
    }
    
    /**
     * 获取底层迭代器
     */
    Iterator& underlying_iterator() {
        check_validity();
        return m_iterator;
    }
    
    /**
     * 获取底层迭代器（const版本）
     */
    const Iterator& underlying_iterator() const {
        check_validity();
        return m_iterator;
    }

private:
    Iterator m_iterator;
    typename LockPolicy::lock_type m_lock;
    bool m_valid;
    
    /**
     * 检查迭代器有效性
     */
    void check_validity() const {
        if (!m_valid) {
            TSL_OM_THROW(invalid_iterator_exception, 
                "Attempt to use invalidated thread-safe iterator");
        }
    }
};

/**
 * 线程安全容器包装器
 * 为容器添加线程安全保护
 */
template <class Container, class LockPolicy = read_write_lock_policy>
class thread_safe_container_wrapper {
public:
    using container_type = Container;
    using lock_policy_type = LockPolicy;
    using size_type = typename Container::size_type;
    using difference_type = typename Container::difference_type;
    using value_type = typename Container::value_type;
    using reference = typename Container::reference;
    using const_reference = typename Container::const_reference;
    using pointer = typename Container::pointer;
    using const_pointer = typename Container::const_pointer;
    using iterator = thread_safe_iterator<typename Container::iterator, LockPolicy>;
    using const_iterator = thread_safe_iterator<typename Container::const_iterator, LockPolicy>;
    
    /**
     * 默认构造函数
     */
    thread_safe_container_wrapper() = default;
    
    /**
     * 构造函数（转发给容器）
     */
    template <class... Args>
    explicit thread_safe_container_wrapper(Args&&... args)
        : m_container(std::forward<Args>(args)...) {}
    
    // 禁止拷贝和移动（可以根据需要添加）
    thread_safe_container_wrapper(const thread_safe_container_wrapper&) = delete;
    thread_safe_container_wrapper& operator=(const thread_safe_container_wrapper&) = delete;
    thread_safe_container_wrapper(thread_safe_container_wrapper&&) = delete;
    thread_safe_container_wrapper& operator=(thread_safe_container_wrapper&&) = delete;
    
    /**
     * 获取容器大小
     */
    TSL_OM_THREAD_SAFE
    size_type size() const {
        auto lock = m_lock_policy.acquire_lock();
        return m_container.size();
    }
    
    /**
     * 检查容器是否为空
     */
    TSL_OM_THREAD_SAFE
    bool empty() const {
        auto lock = m_lock_policy.acquire_lock();
        return m_container.empty();
    }
    
    /**
     * 插入元素
     */
    template <class... Args>
    TSL_OM_THREAD_SAFE
    auto insert(Args&&... args) {
        auto lock = m_lock_policy.acquire_lock();
        return m_container.insert(std::forward<Args>(args)...);
    }
    
    /**
     * 查找元素
     */
    template <class Key>
    TSL_OM_THREAD_SAFE
    iterator find(const Key& key) {
        auto lock = m_lock_policy.acquire_lock();
        return iterator(m_container.find(key), m_lock_policy);
    }
    
    /**
     * 查找元素（const版本）
     */
    template <class Key>
    TSL_OM_THREAD_SAFE
    const_iterator find(const Key& key) const {
        auto lock = m_lock_policy.acquire_lock();
        return const_iterator(m_container.find(key), m_lock_policy);
    }
    
    /**
     * 访问元素
     */
    template <class Key>
    TSL_OM_THREAD_SAFE
    reference operator[](const Key& key) {
        auto lock = m_lock_policy.acquire_lock();
        return m_container[key];
    }
    
    /**
     * 访问元素（const版本）
     */
    template <class Key>
    TSL_OM_THREAD_SAFE
    const_reference at(const Key& key) const {
        auto lock = m_lock_policy.acquire_lock();
        return m_container.at(key);
    }
    
    /**
     * 删除元素
     */
    template <class Key>
    TSL_OM_THREAD_SAFE
    size_type erase(const Key& key) {
        auto lock = m_lock_policy.acquire_lock();
        return m_container.erase(key);
    }
    
    /**
     * 删除元素（通过迭代器）
     */
    TSL_OM_THREAD_SAFE
    iterator erase(const iterator& pos) {
        auto lock = m_lock_policy.acquire_lock();
        // 确保迭代器的锁与当前锁兼容
        auto underlying_pos = pos.underlying_iterator();
        auto new_pos = m_container.erase(underlying_pos);
        return iterator(new_pos, m_lock_policy);
    }
    
    /**
     * 清空容器
     */
    TSL_OM_THREAD_SAFE
    void clear() {
        auto lock = m_lock_policy.acquire_lock();
        m_container.clear();
    }
    
    /**
     * 获取开始迭代器
     */
    TSL_OM_THREAD_SAFE
    iterator begin() {
        auto lock = m_lock_policy.acquire_lock();
        return iterator(m_container.begin(), m_lock_policy);
    }
    
    /**
     * 获取结束迭代器
     */
    TSL_OM_THREAD_SAFE
    iterator end() {
        auto lock = m_lock_policy.acquire_lock();
        return iterator(m_container.end(), m_lock_policy);
    }
    
    /**
     * 获取开始迭代器（const版本）
     */
    TSL_OM_THREAD_SAFE
    const_iterator begin() const {
        auto lock = m_lock_policy.acquire_lock();
        return const_iterator(m_container.begin(), m_lock_policy);
    }
    
    /**
     * 获取结束迭代器（const版本）
     */
    TSL_OM_THREAD_SAFE
    const_iterator end() const {
        auto lock = m_lock_policy.acquire_lock();
        return const_iterator(m_container.end(), m_lock_policy);
    }
    
    /**
     * 获取开始迭代器（const版本）
     */
    TSL_OM_THREAD_SAFE
    const_iterator cbegin() const {
        return begin();
    }
    
    /**
     * 获取结束迭代器（const版本）
     */
    TSL_OM_THREAD_SAFE
    const_iterator cend() const {
        return end();
    }
    
    /**
     * 交换容器内容
     */
    TSL_OM_THREAD_SAFE
    void swap(thread_safe_container_wrapper& other) {
        // 确保以一致的顺序获取两个锁以避免死锁
        if (this < &other) {
            auto lock1 = m_lock_policy.acquire_lock();
            auto lock2 = other.m_lock_policy.acquire_lock();
            m_container.swap(other.m_container);
        } else if (this > &other) {
            auto lock2 = other.m_lock_policy.acquire_lock();
            auto lock1 = m_lock_policy.acquire_lock();
            m_container.swap(other.m_container);
        }
        // 如果是同一个对象，不做任何操作
    }
    
    /**
     * 获取底层容器（需要谨慎使用）
     */
    TSL_OM_THREAD_SAFE
    Container& underlying_container() {
        // 注意：返回底层容器会绕过线程安全保护
        // 仅在确保线程安全的情况下使用
        return m_container;
    }
    
    /**
     * 获取底层容器（const版本）
     */
    TSL_OM_THREAD_SAFE
    const Container& underlying_container() const {
        return m_container;
    }
    
    /**
     * 获取锁策略
     */
    LockPolicy& lock_policy() noexcept {
        return m_lock_policy;
    }
    
    /**
     * 获取锁策略（const版本）
     */
    const LockPolicy& lock_policy() const noexcept {
        return m_lock_policy;
    }

private:
    Container m_container;
    mutable LockPolicy m_lock_policy;
};

/**
 * 线程安全迭代器适配器
 * 用于将普通迭代器转换为线程安全迭代器
 */
template <class Iterator, class LockPolicy>
thread_safe_iterator<Iterator, LockPolicy>
make_thread_safe_iterator(Iterator it, const LockPolicy& lock_policy) {
    return thread_safe_iterator<Iterator, LockPolicy>(std::move(it), lock_policy);
}

/**
 * 线程本地存储工具类
 * 用于存储线程私有状态
 */
template <class T>
class thread_local_storage {
public:
    /**
     * 获取当前线程的实例
     */
    static T& instance() noexcept {
        thread_local T instance;
        return instance;
    }
    
    /**
     * 检查当前线程是否有实例
     */
    static bool has_instance() noexcept {
        try {
            // 尝试访问实例，如果没有则会创建
            instance();
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /**
     * 销毁当前线程的实例
     */
    static void destroy_instance() noexcept {
        // 线程本地存储的实例会在线程结束时自动销毁
        // 这里不需要做任何事情
    }
};

/**
 * 原子操作工具类
 * 提供原子操作的便捷接口
 */
class atomic_utils {
public:
    /**
     * 原子递增操作
     */
    template <class T>
    static T atomic_increment(std::atomic<T>& atomic_var) noexcept {
        return atomic_var.fetch_add(1, std::memory_order_relaxed) + 1;
    }
    
    /**
     * 原子递减操作
     */
    template <class T>
    static T atomic_decrement(std::atomic<T>& atomic_var) noexcept {
        return atomic_var.fetch_sub(1, std::memory_order_relaxed) - 1;
    }
    
    /**
     * 原子比较并交换操作
     */
    template <class T>
    static bool atomic_compare_exchange(std::atomic<T>& atomic_var, 
                                      T& expected, T desired) noexcept {
        return atomic_var.compare_exchange_weak(expected, desired, 
                                               std::memory_order_release, 
                                               std::memory_order_relaxed);
    }
    
    /**
     * 原子加载操作
     */
    template <class T>
    static T atomic_load(const std::atomic<T>& atomic_var) noexcept {
        return atomic_var.load(std::memory_order_relaxed);
    }
    
    /**
     * 原子存储操作
     */
    template <class T>
    static void atomic_store(std::atomic<T>& atomic_var, T value) noexcept {
        atomic_var.store(value, std::memory_order_release);
    }
};

} // namespace detail_ordered_hash

} // namespace tsl

#endif // TSL_ORDERED_MAP_CONCURRENT_H
