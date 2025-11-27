#ifndef TSL_ORDERED_MAP_WITH_EXCEPTIONS_H
#define TSL_ORDERED_MAP_WITH_EXCEPTIONS_H

#include "ordered_map.h"
#include "ordered_map_exceptions.h"
#include "ordered_map_memory.h"
#include "ordered_map_network.h"
#include "ordered_map_concurrent.h"
#include "ordered_map_exception_info.h"

namespace tsl {

/**
 * 增强型ordered_map类
 * 集成了全面的异常安全防护机制
 */
template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>,
          bool StoreHash = false,
          class GrowthPolicy = tsl::hh::power_of_two_growth_policy,
          class LockPolicy = detail_ordered_hash::read_write_lock_policy>
class ordered_map_with_exceptions : 
    public ordered_map<Key, T, Hash, KeyEqual, 
                      tracking_allocator<std::pair<const Key, T>, Allocator>, 
                      StoreHash, GrowthPolicy> {
public:
    using base_type = ordered_map<Key, T, Hash, KeyEqual, 
                                 tracking_allocator<std::pair<const Key, T>, Allocator>, 
                                 StoreHash, GrowthPolicy>;
    using key_type = typename base_type::key_type;
    using mapped_type = typename base_type::mapped_type;
    using value_type = typename base_type::value_type;
    using size_type = typename base_type::size_type;
    using difference_type = typename base_type::difference_type;
    using hasher = typename base_type::hasher;
    using key_equal = typename base_type::key_equal;
    using allocator_type = typename base_type::allocator_type;
    using reference = typename base_type::reference;
    using const_reference = typename base_type::const_reference;
    using pointer = typename base_type::pointer;
    using const_pointer = typename base_type::const_pointer;
    using iterator = typename base_type::iterator;
    using const_iterator = typename base_type::const_iterator;
    using local_iterator = typename base_type::local_iterator;
    using const_local_iterator = typename base_type::const_local_iterator;
    using node_type = typename base_type::node_type;
    using insert_return_type = typename base_type::insert_return_type;
    
    /**
     * 默认构造函数
     */
    ordered_map_with_exceptions() : base_type() {
        init();
    }
    
    /**
     * 构造函数（带哈希函数和键比较函数）
     */
    explicit ordered_map_with_exceptions(size_type bucket_count,
                                        const hasher& hash = hasher(),
                                        const key_equal& equal = key_equal(),
                                        const allocator_type& alloc = allocator_type())
        : base_type(bucket_count, hash, equal, alloc) {
        init();
    }
    
    /**
     * 构造函数（带分配器）
     */
    explicit ordered_map_with_exceptions(const allocator_type& alloc)
        : base_type(alloc) {
        init();
    }
    
    /**
     * 构造函数（带范围）
     */
    template <class InputIt>
    ordered_map_with_exceptions(InputIt first, InputIt last,
                               size_type bucket_count = 0,
                               const hasher& hash = hasher(),
                               const key_equal& equal = key_equal(),
                               const allocator_type& alloc = allocator_type())
        : base_type(first, last, bucket_count, hash, equal, alloc) {
        init();
    }
    
    /**
     * 拷贝构造函数
     */
    ordered_map_with_exceptions(const ordered_map_with_exceptions& other)
        : base_type(other) {
        init();
        m_memory_manager = other.m_memory_manager;
    }
    
    /**
     * 移动构造函数
     */
    ordered_map_with_exceptions(ordered_map_with_exceptions&& other) noexcept
        : base_type(std::move(other)) {
        init();
        m_memory_manager = std::move(other.m_memory_manager);
    }
    
    /**
     * 初始化列表构造函数
     */
    ordered_map_with_exceptions(std::initializer_list<value_type> init,
                               size_type bucket_count = 0,
                               const hasher& hash = hasher(),
                               const key_equal& equal = key_equal(),
                               const allocator_type& alloc = allocator_type())
        : base_type(init, bucket_count, hash, equal, alloc) {
        init();
    }
    
    /**
     * 拷贝赋值运算符
     */
    ordered_map_with_exceptions& operator=(const ordered_map_with_exceptions& other) {
        if (this != &other) {
            base_type::operator=(other);
            m_memory_manager = other.m_memory_manager;
        }
        return *this;
    }
    
    /**
     * 移动赋值运算符
     */
    ordered_map_with_exceptions& operator=(ordered_map_with_exceptions&& other) noexcept {
        if (this != &other) {
            base_type::operator=(std::move(other));
            m_memory_manager = std::move(other.m_memory_manager);
        }
        return *this;
    }
    
    /**
     * 初始化列表赋值运算符
     */
    ordered_map_with_exceptions& operator=(std::initializer_list<value_type> init) {
        base_type::operator=(init);
        return *this;
    }
    
    /**
     * 析构函数
     */
    ~ordered_map_with_exceptions() override = default;
    
    /**
     * 插入元素（带空指针检查）
     */
    std::pair<iterator, bool> insert(const value_type& value) {
        check_key_validity(value.first);
        check_hash_function();
        check_key_equal_function();
        
        auto lock = m_lock_policy.acquire_lock();
        try {
            auto result = base_type::insert(value);
            check_memory_usage();
            return result;
        } catch (const std::bad_alloc& e) {
            handle_memory_allocation_failure(e);
            throw;
        } catch (...) {
            throw;
        }
    }
    
    /**
     * 插入元素（移动版本）
     */
    std::pair<iterator, bool> insert(value_type&& value) {
        check_key_validity(value.first);
        check_hash_function();
        check_key_equal_function();
        
        auto lock = m_lock_policy.acquire_lock();
        try {
            auto result = base_type::insert(std::move(value));
            check_memory_usage();
            return result;
        } catch (const std::bad_alloc& e) {
            handle_memory_allocation_failure(e);
            throw;
        } catch (...) {
            throw;
        }
    }
    
    /**
     * 插入元素（带位置提示）
     */
    iterator insert(const_iterator hint, const value_type& value) {
        check_key_validity(value.first);
        check_hash_function();
        check_key_equal_function();
        check_iterator_validity(hint);
        
        auto lock = m_lock_policy.acquire_lock();
        try {
            auto result = base_type::insert(hint, value);
            check_memory_usage();
            return result;
        } catch (const std::bad_alloc& e) {
            handle_memory_allocation_failure(e);
            throw;
        } catch (...) {
            throw;
        }
    }
    
    /**
     * 插入元素（带位置提示和移动）
     */
    iterator insert(const_iterator hint, value_type&& value) {
        check_key_validity(value.first);
        check_hash_function();
        check_key_equal_function();
        check_iterator_validity(hint);
        
        auto lock = m_lock_policy.acquire_lock();
        try {
            auto result = base_type::insert(hint, std::move(value));
            check_memory_usage();
            return result;
        } catch (const std::bad_alloc& e) {
            handle_memory_allocation_failure(e);
            throw;
        } catch (...) {
            throw;
        }
    }
    
    /**
     * 插入范围
     */
    template <class InputIt>
    void insert(InputIt first, InputIt last) {
        check_hash_function();
        check_key_equal_function();
        
        auto lock = m_lock_policy.acquire_lock();
        try {
            base_type::insert(first, last);
            check_memory_usage();
        } catch (const std::bad_alloc& e) {
            handle_memory_allocation_failure(e);
            throw;
        } catch (...) {
            throw;
        }
    }
    
    /**
     * 插入初始化列表
     */
    void insert(std::initializer_list<value_type> init) {
        check_hash_function();
        check_key_equal_function();
        
        auto lock = m_lock_policy.acquire_lock();
        try {
            base_type::insert(init);
            check_memory_usage();
        } catch (const std::bad_alloc& e) {
            handle_memory_allocation_failure(e);
            throw;
        } catch (...) {
            throw;
        }
    }
    
    /**
     * 查找元素（带空指针检查）
     */
    iterator find(const key_type& key) {
        check_key_validity(key);
        check_hash_function();
        check_key_equal_function();
        
        auto lock = m_lock_policy.acquire_lock();
        return base_type::find(key);
    }
    
    /**
     * 查找元素（const版本）
     */
    const_iterator find(const key_type& key) const {
        check_key_validity(key);
        check_hash_function();
        check_key_equal_function();
        
        auto lock = m_lock_policy.acquire_lock();
        return base_type::find(key);
    }
    
    /**
     * 访问元素（带空指针检查）
     */
    mapped_type& operator[](const key_type& key) {
        check_key_validity(key);
        check_hash_function();
        check_key_equal_function();
        
        auto lock = m_lock_policy.acquire_lock();
        try {
            auto& result = base_type::operator[](key);
            check_memory_usage();
            return result;
        } catch (const std::bad_alloc& e) {
            handle_memory_allocation_failure(e);
            throw;
        } catch (...) {
            throw;
        }
    }
    
    /**
     * 访问元素（移动版本）
     */
    mapped_type& operator[](key_type&& key) {
        check_key_validity(key);
        check_hash_function();
        check_key_equal_function();
        
        auto lock = m_lock_policy.acquire_lock();
        try {
            auto& result = base_type::operator[](std::move(key));
            check_memory_usage();
            return result;
        } catch (const std::bad_alloc& e) {
            handle_memory_allocation_failure(e);
            throw;
        } catch (...) {
            throw;
        }
    }
    
    /**
     * 访问元素（带边界检查）
     */
    mapped_type& at(const key_type& key) {
        check_key_validity(key);
        check_hash_function();
        check_key_equal_function();
        
        auto lock = m_lock_policy.acquire_lock();
        try {
            return base_type::at(key);
        } catch (const std::out_of_range& e) {
            TSL_OM_ENHANCED_THROW_WITH_STATE(out_of_range_exception, 
                "Key not found in ordered_map", *this);
        } catch (...) {
            throw;
        }
    }
    
    /**
     * 访问元素（const版本，带边界检查）
     */
    const mapped_type& at(const key_type& key) const {
        check_key_validity(key);
        check_hash_function();
        check_key_equal_function();
        
        auto lock = m_lock_policy.acquire_lock();
        try {
            return base_type::at(key);
        } catch (const std::out_of_range& e) {
            TSL_OM_ENHANCED_THROW_WITH_STATE(out_of_range_exception, 
                "Key not found in ordered_map", *this);
        } catch (...) {
            throw;
        }
    }
    
    /**
     * 擦除元素（带空指针检查）
     */
    size_type erase(const key_type& key) {
        check_key_validity(key);
        check_hash_function();
        check_key_equal_function();
        
        auto lock = m_lock_policy.acquire_lock();
        return base_type::erase(key);
    }
    
    /**
     * 擦除元素（通过迭代器）
     */
    iterator erase(const_iterator pos) {
        check_iterator_validity(pos);
        
        auto lock = m_lock_policy.acquire_lock();
        return base_type::erase(pos);
    }
    
    /**
     * 擦除元素（通过范围）
     */
    iterator erase(const_iterator first, const_iterator last) {
        check_iterator_validity(first);
        check_iterator_validity(last);
        
        auto lock = m_lock_policy.acquire_lock();
        return base_type::erase(first, last);
    }
    
    /**
     * 清空容器
     */
    void clear() noexcept {
        auto lock = m_lock_policy.acquire_lock();
        base_type::clear();
    }
    
    /**
     * 交换容器内容
     */
    void swap(ordered_map_with_exceptions& other) {
        // 确保以一致的顺序获取两个锁以避免死锁
        if (this < &other) {
            auto lock1 = m_lock_policy.acquire_lock();
            auto lock2 = other.m_lock_policy.acquire_lock();
            base_type::swap(other);
            std::swap(m_memory_manager, other.m_memory_manager);
        } else if (this > &other) {
            auto lock2 = other.m_lock_policy.acquire_lock();
            auto lock1 = m_lock_policy.acquire_lock();
            base_type::swap(other);
            std::swap(m_memory_manager, other.m_memory_manager);
        }
        // 如果是同一个对象，不做任何操作
    }
    
    /**
     * 设置内存使用上限
     */
    void set_memory_limit(std::size_t limit_bytes) {
        m_memory_manager.set_memory_limit(limit_bytes);
    }
    
    /**
     * 获取当前内存使用上限
     */
    std::size_t memory_limit() const noexcept {
        return m_memory_manager.memory_limit();
    }
    
    /**
     * 获取当前内存使用量
     */
    std::size_t current_memory_usage() const noexcept {
        return m_memory_manager.current_memory_usage();
    }
    
    /**
     * 设置内存碎片率阈值
     */
    void set_fragmentation_threshold(float threshold) {
        m_memory_manager.set_fragmentation_threshold(threshold);
    }
    
    /**
     * 获取内存碎片率阈值
     */
    float fragmentation_threshold() const noexcept {
        return m_memory_manager.fragmentation_threshold();
    }
    
    /**
     * 手动触发内存整理
     */
    void defragment_memory() {
        auto lock = m_lock_policy.acquire_lock();
        m_memory_manager.defragment_memory();
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
    
    /**
     * 安全序列化（带超时、重试和CRC校验）
     */
    template <class Serializer>
    void serialize_safe(Serializer&& serializer,
                       std::chrono::milliseconds timeout = std::chrono::milliseconds(30000),
                       const detail_ordered_hash::network_retry_strategy& retry_strategy = 
                           detail_ordered_hash::network_retry_strategy()) const {
        auto lock = m_lock_policy.acquire_lock();
        
        detail_ordered_hash::safe_serializer<Serializer> safe_serializer(
            std::forward<Serializer>(serializer), timeout, retry_strategy
        );
        
        // 序列化容器状态
        safe_serializer.serialize_with_crc(size());
        safe_serializer.serialize_with_crc(max_size());
        safe_serializer.serialize_with_crc(bucket_count());
        
        // 序列化元素
        for (const auto& element : *this) {
            safe_serializer.serialize_with_crc(element);
        }
    }
    
    /**
     * 安全反序列化（带超时、重试和CRC校验）
     */
    template <class Deserializer>
    void deserialize_safe(Deserializer&& deserializer,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds(30000),
                         const detail_ordered_hash::network_retry_strategy& retry_strategy = 
                             detail_ordered_hash::network_retry_strategy()) {
        auto lock = m_lock_policy.acquire_lock();
        
        detail_ordered_hash::safe_deserializer<Deserializer> safe_deserializer(
            std::forward<Deserializer>(deserializer), timeout, retry_strategy
        );
        
        // 清空现有内容
        clear();
        
        // 反序列化容器状态
        const size_type size = safe_deserializer.deserialize_with_crc<size_type>();
        const size_type max_size = safe_deserializer.deserialize_with_crc<size_type>();
        const size_type bucket_count = safe_deserializer.deserialize_with_crc<size_type>();
        
        // 预留空间
        reserve(bucket_count);
        
        // 反序列化元素
        for (size_type i = 0; i < size; ++i) {
            const value_type element = safe_deserializer.deserialize_with_crc<value_type>();
            insert(element);
        }
    }
    
    /**
     * 获取线程安全的开始迭代器
     */
    detail_ordered_hash::thread_safe_iterator<iterator, LockPolicy> thread_safe_begin() {
        return detail_ordered_hash::make_thread_safe_iterator(begin(), m_lock_policy);
    }
    
    /**
     * 获取线程安全的结束迭代器
     */
    detail_ordered_hash::thread_safe_iterator<iterator, LockPolicy> thread_safe_end() {
        return detail_ordered_hash::make_thread_safe_iterator(end(), m_lock_policy);
    }
    
    /**
     * 获取线程安全的开始迭代器（const版本）
     */
    detail_ordered_hash::thread_safe_iterator<const_iterator, LockPolicy> thread_safe_begin() const {
        return detail_ordered_hash::make_thread_safe_iterator(begin(), m_lock_policy);
    }
    
    /**
     * 获取线程安全的结束迭代器（const版本）
     */
    detail_ordered_hash::thread_safe_iterator<const_iterator, LockPolicy> thread_safe_end() const {
        return detail_ordered_hash::make_thread_safe_iterator(end(), m_lock_policy);
    }
    
    /**
     * 获取线程安全的开始迭代器（const版本）
     */
    detail_ordered_hash::thread_safe_iterator<const_iterator, LockPolicy> thread_safe_cbegin() const {
        return thread_safe_begin();
    }
    
    /**
     * 获取线程安全的结束迭代器（const版本）
     */
    detail_ordered_hash::thread_safe_iterator<const_iterator, LockPolicy> thread_safe_cend() const {
        return thread_safe_end();
    }
    
protected:
    /**
     * 初始化
     */
    void init() {
        // 设置内存管理器的分配器
        m_memory_manager.set_allocator(&this->get_allocator());
        
        // 注册内存分配器的回调
        this->get_allocator().set_allocation_callback(
            [this](std::size_t size) {
                m_memory_manager.on_allocation(size);
            }
        );
        
        this->get_allocator().set_deallocation_callback(
            [this](std::size_t size) {
                m_memory_manager.on_deallocation(size);
            }
        );
    }
    
    /**
     * 检查键的有效性
     */
    void check_key_validity(const key_type& key) const {
        // 对于指针类型的键，检查是否为空
        if constexpr (std::is_pointer_v<key_type>) {
            if (key == nullptr) {
                TSL_OM_ENHANCED_THROW_WITH_STATE(null_pointer_exception, 
                    "Null pointer key provided to ordered_map", *this);
            }
        }
    }
    
    /**
     * 检查哈希函数的有效性
     */
    void check_hash_function() const {
        // 检查哈希函数是否可调用
        try {
            if constexpr (!std::is_default_constructible_v<hasher>) {
                // 如果哈希函数不是默认构造的，检查是否已初始化
                // 这里需要根据具体的哈希函数类型进行调整
            }
        } catch (...) {
            TSL_OM_ENHANCED_THROW_WITH_STATE(uninitialized_function_exception, 
                "Hash function is not properly initialized", *this);
        }
    }
    
    /**
     * 检查键比较函数的有效性
     */
    void check_key_equal_function() const {
        // 检查键比较函数是否可调用
        try {
            if constexpr (!std::is_default_constructible_v<key_equal>) {
                // 如果键比较函数不是默认构造的，检查是否已初始化
                // 这里需要根据具体的键比较函数类型进行调整
            }
        } catch (...) {
            TSL_OM_ENHANCED_THROW_WITH_STATE(uninitialized_function_exception, 
                "Key equal function is not properly initialized", *this);
        }
    }
    
    /**
     * 检查迭代器的有效性
     */
    void check_iterator_validity(const const_iterator& it) const {
        // 检查迭代器是否有效
        if (it == end() || it == begin()) {
            // 对于end()和begin()迭代器，简单检查即可
            return;
        }
        
        // 更详细的迭代器有效性检查
        try {
            // 尝试解引用迭代器来检查有效性
            const_cast<ordered_map_with_exceptions*>(this)->find(it->first);
        } catch (...) {
            TSL_OM_ENHANCED_THROW_WITH_STATE(invalid_iterator_exception, 
                "Invalid iterator provided to ordered_map", *this);
        }
    }
    
    /**
     * 检查内存使用情况
     */
    void check_memory_usage() {
        try {
            m_memory_manager.check_memory_usage();
        } catch (const memory_limit_exception& e) {
            // 尝试进行LRU淘汰
            if (m_memory_manager.evict_lru_elements(10)) { // 淘汰10个最近最少使用的元素
                return; // 淘汰成功，继续操作
            }
            // 淘汰失败，重新抛出异常
            throw;
        }
    }
    
    /**
     * 处理内存分配失败
     */
    void handle_memory_allocation_failure(const std::bad_alloc& e) {
        try {
            // 尝试进行内存整理
            m_memory_manager.defragment_memory();
            
            // 再次尝试分配（这里需要根据具体情况调整）
            // 如果还是失败，会再次抛出bad_alloc
        } catch (...) {
            // 内存整理也失败，抛出增强型异常
            TSL_OM_ENHANCED_THROW_WITH_STATE(memory_allocation_exception, 
                std::string("Memory allocation failed: ") + e.what(), *this);
        }
    }
    
    mutable LockPolicy m_lock_policy;
    detail_ordered_hash::memory_manager m_memory_manager;
    
    /**
     * 检查容器是否有load_factor方法的辅助模板
     */
    template <class T>
    static auto test_load_factor(int) -> decltype(std::declval<T>().load_factor(), std::true_type());
    
    template <class T>
    static std::false_type test_load_factor(...);
    
    static constexpr bool has_load_factor_v = decltype(test_load_factor<base_type>(0))::value;
    
    /**
     * 检查容器是否有bucket_count方法的辅助模板
     */
    template <class T>
    static auto test_bucket_count(int) -> decltype(std::declval<T>().bucket_count(), std::true_type());
    
    template <class T>
    static std::false_type test_bucket_count(...);
    
    static constexpr bool has_bucket_count_v = decltype(test_bucket_count<base_type>(0))::value;
};

/**
 * 交换函数（非成员版本）
 */
template <class Key, class T, class Hash, class KeyEqual, class Allocator, 
          bool StoreHash, class GrowthPolicy, class LockPolicy>
void swap(ordered_map_with_exceptions<Key, T, Hash, KeyEqual, Allocator, 
                                     StoreHash, GrowthPolicy, LockPolicy>& lhs,
         ordered_map_with_exceptions<Key, T, Hash, KeyEqual, Allocator, 
                                     StoreHash, GrowthPolicy, LockPolicy>& rhs) {
    lhs.swap(rhs);
}

/**
 * 线程安全的ordered_map类型定义
 */
template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>,
          bool StoreHash = false,
          class GrowthPolicy = tsl::hh::power_of_two_growth_policy>
typedef ordered_map_with_exceptions<Key, T, Hash, KeyEqual, Allocator, 
                                    StoreHash, GrowthPolicy, 
                                    detail_ordered_hash::read_write_lock_policy>
    thread_safe_ordered_map;

/**
 * 无锁的ordered_map类型定义（用于单线程环境）
 */
template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>,
          bool StoreHash = false,
          class GrowthPolicy = tsl::hh::power_of_two_growth_policy>
typedef ordered_map_with_exceptions<Key, T, Hash, KeyEqual, Allocator, 
                                    StoreHash, GrowthPolicy, 
                                    detail_ordered_hash::no_lock_policy>
    lock_free_ordered_map;

} // namespace tsl

#endif // TSL_ORDERED_MAP_WITH_EXCEPTIONS_H
