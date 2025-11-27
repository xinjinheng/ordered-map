#ifndef TSL_ORDERED_MAP_MEMORY_H
#define TSL_ORDERED_MAP_MEMORY_H

#include <cstddef>
#include <limits>
#include <list>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include "ordered_map_exceptions.h"

namespace tsl {

namespace detail_ordered_hash {

/**
 * 内存跟踪分配器
 * 用于跟踪内存使用情况并在分配失败时提供优雅的错误处理
 */
template <class T, class BaseAllocator = std::allocator<T>>
class tracking_allocator : public BaseAllocator {
public:
    using value_type = typename BaseAllocator::value_type;
    using pointer = typename BaseAllocator::pointer;
    using const_pointer = typename BaseAllocator::const_pointer;
    using reference = typename BaseAllocator::reference;
    using const_reference = typename BaseAllocator::const_reference;
    using size_type = typename BaseAllocator::size_type;
    using difference_type = typename BaseAllocator::difference_type;
    
    template <class U>
    struct rebind {
        using other = tracking_allocator<U, typename BaseAllocator::template rebind<U>::other>;
    };

    tracking_allocator() noexcept : m_memory_limit(std::numeric_limits<size_type>::max()) {}
    
    explicit tracking_allocator(size_type memory_limit) noexcept 
        : m_memory_limit(memory_limit) {}
    
    template <class U>
    tracking_allocator(const tracking_allocator<U>& other) noexcept 
        : BaseAllocator(other), m_memory_limit(other.memory_limit()) {}

    pointer allocate(size_type n, const void* hint = nullptr) {
        const size_type required_size = n * sizeof(value_type);
        
        if (required_size > max_size()) {
            TSL_OM_THROW(memory_allocation_exception, 
                "Memory allocation failed: Requested size exceeds maximum allowed size");
        }
        
        try {
            pointer ptr = BaseAllocator::allocate(n, hint);
            m_total_allocated += required_size;
            return ptr;
        } catch (const std::bad_alloc&) {
            TSL_OM_THROW(memory_allocation_exception, 
                "Memory allocation failed: Insufficient memory");
        }
    }

    void deallocate(pointer ptr, size_type n) {
        const size_type deallocated_size = n * sizeof(value_type);
        BaseAllocator::deallocate(ptr, n);
        
        if (m_total_allocated >= deallocated_size) {
            m_total_allocated -= deallocated_size;
        } else {
            m_total_allocated = 0;
        }
    }

    size_type max_size() const noexcept {
        return std::min(BaseAllocator::max_size(), 
                       m_memory_limit / sizeof(value_type));
    }

    size_type memory_limit() const noexcept {
        return m_memory_limit;
    }

    void set_memory_limit(size_type limit) noexcept {
        m_memory_limit = limit;
    }

    size_type total_allocated() const noexcept {
        return m_total_allocated;
    }

private:
    size_type m_memory_limit; 
    static thread_local size_type m_total_allocated; // 线程局部存储，跟踪每个线程的内存分配
};

template <class T, class BaseAllocator>
thread_local typename tracking_allocator<T, BaseAllocator>::size_type 
tracking_allocator<T, BaseAllocator>::m_total_allocated = 0;

/**
 * LRU（最近最少使用）淘汰策略
 * 用于在内存使用超过上限时淘汰最不常用的元素
 */
template <class Key, class Value>
class lru_eviction_policy {
public:
    using key_type = Key;
    using value_type = Value;
    
    /**
     * 记录元素访问
     */
    void touch(const key_type& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 如果键已存在，将其移动到列表开头
        auto it = m_key_to_iter.find(key);
        if (it != m_key_to_iter.end()) {
            m_lru_list.erase(it->second);
            m_lru_list.push_front(key);
            it->second = m_lru_list.begin();
        } else {
            // 新键添加到列表开头
            m_lru_list.push_front(key);
            m_key_to_iter[key] = m_lru_list.begin();
        }
    }
    
    /**
     * 获取要淘汰的键（最近最少使用的键）
     */
    bool get_eviction_key(key_type& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_lru_list.empty()) {
            return false;
        }
        
        key = m_lru_list.back();
        m_lru_list.pop_back();
        m_key_to_iter.erase(key);
        
        return true;
    }
    
    /**
     * 移除指定键
     */
    void remove(const key_type& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_key_to_iter.find(key);
        if (it != m_key_to_iter.end()) {
            m_lru_list.erase(it->second);
            m_key_to_iter.erase(it);
        }
    }
    
    /**
     * 清空所有记录
     */
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lru_list.clear();
        m_key_to_iter.clear();
    }
    
    /**
     * 获取LRU列表的大小
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lru_list.size();
    }

private:
    mutable std::mutex m_mutex; // 保护内部数据结构的互斥锁
    std::list<key_type> m_lru_list; // LRU列表，开头是最近使用的元素
    std::unordered_map<key_type, typename std::list<key_type>::iterator> m_key_to_iter; // 键到迭代器的映射
};

/**
 * 内存碎片检测器
 * 用于检测和处理内存碎片
 */
class memory_fragmentation_detector {
public:
    memory_fragmentation_detector() 
        : m_fragmentation_threshold(20.0f), m_check_interval(1000) {} // 默认阈值20%，每1000次操作检查一次
    
    explicit memory_fragmentation_detector(float threshold, size_t check_interval) 
        : m_fragmentation_threshold(threshold), m_check_interval(check_interval) {}
    
    /**
     * 记录内存分配
     */
    void record_allocation(size_t size) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_total_allocated += size;
        m_allocation_count++;
        
        // 检查是否需要进行碎片检测
        if (m_allocation_count % m_check_interval == 0) {
            check_fragmentation();
        }
    }
    
    /**
     * 记录内存释放
     */
    void record_deallocation(size_t size) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_total_allocated >= size) {
            m_total_allocated -= size;
        } else {
            m_total_allocated = 0;
        }
        m_free_memory += size;
    }
    
    /**
     * 检查是否需要进行内存整理
     */
    bool needs_defragmentation() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_needs_defragmentation;
    }
    
    /**
     * 重置碎片检测状态
     */
    void reset_defragmentation_flag() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_needs_defragmentation = false;
    }
    
    /**
     * 获取当前碎片率
     */
    float fragmentation_rate() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_total_allocated == 0) {
            return 0.0f;
        }
        
        // 简化的碎片率计算：空闲内存 / (已分配内存 + 空闲内存)
        const float total_memory = m_total_allocated + m_free_memory;
        return (m_free_memory / total_memory) * 100.0f;
    }
    
    /**
     * 设置碎片率阈值
     */
    void set_fragmentation_threshold(float threshold) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fragmentation_threshold = threshold;
    }
    
    /**
     * 设置检查间隔
     */
    void set_check_interval(size_t interval) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_check_interval = interval;
    }

private:
    mutable std::mutex m_mutex;
    float m_fragmentation_threshold; // 碎片率阈值（百分比）
    size_t m_check_interval; // 检查间隔（分配次数）
    
    size_t m_total_allocated = 0; // 总已分配内存
    size_t m_free_memory = 0; // 总空闲内存
    size_t m_allocation_count = 0; // 分配次数计数器
    bool m_needs_defragmentation = false; // 是否需要内存整理
    
    /**
     * 检查内存碎片情况
     */
    void check_fragmentation() {
        const float current_fragmentation = fragmentation_rate();
        m_needs_defragmentation = (current_fragmentation > m_fragmentation_threshold);
    }
};

/**
 * 内存管理控制器
 * 整合内存跟踪、LRU淘汰和碎片检测功能
 */
template <class Key, class Value, class Allocator = std::allocator<std::pair<Key, Value>>>
class memory_manager {
public:
    using key_type = Key;
    using value_type = Value;
    using allocator_type = Allocator;
    using size_type = std::size_t;
    
    explicit memory_manager(size_type memory_limit = std::numeric_limits<size_type>::max())
        : m_memory_limit(memory_limit), m_allocator(memory_limit),
          m_fragmentation_detector() {}
    
    memory_manager(size_type memory_limit, float fragmentation_threshold, size_t check_interval)
        : m_memory_limit(memory_limit), m_allocator(memory_limit),
          m_fragmentation_detector(fragmentation_threshold, check_interval) {}
    
    /**
     * 分配内存
     */
    template <class T>
    T* allocate(size_t n) {
        const size_t required_size = n * sizeof(T);
        
        // 检查内存使用是否超过上限
        if (m_allocator.total_allocated() + required_size > m_memory_limit) {
            return nullptr; // 需要进行LRU淘汰
        }
        
        try {
            T* ptr = m_allocator.template allocate<T>(n);
            m_fragmentation_detector.record_allocation(required_size);
            return ptr;
        } catch (const memory_allocation_exception&) {
            return nullptr;
        }
    }
    
    /**
     * 释放内存
     */
    template <class T>
    void deallocate(T* ptr, size_t n) {
        const size_t deallocated_size = n * sizeof(T);
        m_allocator.template deallocate<T>(ptr, n);
        m_fragmentation_detector.record_deallocation(deallocated_size);
    }
    
    /**
     * 记录元素访问（用于LRU）
     */
    void touch(const key_type& key) {
        m_lru_policy.touch(key);
    }
    
    /**
     * 获取要淘汰的键
     */
    bool get_eviction_key(key_type& key) {
        return m_lru_policy.get_eviction_key(key);
    }
    
    /**
     * 移除指定键的LRU记录
     */
    void remove_from_lru(const key_type& key) {
        m_lru_policy.remove(key);
    }
    
    /**
     * 检查是否需要内存整理
     */
    bool needs_defragmentation() const {
        return m_fragmentation_detector.needs_defragmentation();
    }
    
    /**
     * 重置碎片检测状态
     */
    void reset_defragmentation_flag() {
        m_fragmentation_detector.reset_defragmentation_flag();
    }
    
    /**
     * 设置内存使用上限
     */
    void set_memory_limit(size_type limit) {
        m_memory_limit = limit;
        m_allocator.set_memory_limit(limit);
    }
    
    /**
     * 获取内存使用上限
     */
    size_type memory_limit() const noexcept {
        return m_memory_limit;
    }
    
    /**
     * 获取当前内存使用量
     */
    size_type current_memory_usage() const noexcept {
        return m_allocator.total_allocated();
    }
    
    /**
     * 获取当前碎片率
     */
    float fragmentation_rate() const {
        return m_fragmentation_detector.fragmentation_rate();
    }
    
    /**
     * 清空所有内存管理状态
     */
    void clear() {
        m_lru_policy.clear();
        // 注意：allocator的状态由其自身管理
    }

private:
    size_type m_memory_limit; // 内存使用上限
    tracking_allocator<value_type, allocator_type> m_allocator; // 内存跟踪分配器
    lru_eviction_policy<key_type, value_type> m_lru_policy; // LRU淘汰策略
    memory_fragmentation_detector m_fragmentation_detector; // 内存碎片检测器
};

} // namespace detail_ordered_hash

} // namespace tsl

#endif // TSL_ORDERED_MAP_MEMORY_H
