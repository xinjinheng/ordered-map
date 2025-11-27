#ifndef TSL_ORDERED_MAP_EXCEPTION_INFO_H
#define TSL_ORDERED_MAP_EXCEPTION_INFO_H

#include <string>
#include <chrono>
#include <sstream>
#include <vector>
#include <map>
#include <typeinfo>
#include <memory>
#include "ordered_map_exceptions.h"

namespace tsl {

namespace detail_ordered_hash {

/**
 * 容器状态快照
 * 用于记录异常发生时容器的状态信息
 */
template <class MapType>
struct container_state_snapshot {
    using size_type = typename MapType::size_type;
    using difference_type = typename MapType::difference_type;
    
    /**
     * 构造函数
     */
    explicit container_state_snapshot(const MapType& map) {
        try {
            size = map.size();
            max_size = map.max_size();
            empty = map.empty();
            
            // 如果容器支持，获取负载因子
            if constexpr (has_load_factor_v<MapType>) {
                load_factor = map.load_factor();
                max_load_factor = map.max_load_factor();
            }
            
            // 如果容器支持，获取桶的数量
            if constexpr (has_bucket_count_v<MapType>) {
                bucket_count = map.bucket_count();
                max_bucket_count = map.max_bucket_count();
            }
        } catch (...) {
            // 捕获所有异常，避免在获取状态时再次抛出异常
            size = 0;
            max_size = 0;
            empty = true;
            load_factor = 0.0f;
            max_load_factor = 0.0f;
            bucket_count = 0;
            max_bucket_count = 0;
        }
    }
    
    /**
     * 转换为字符串表示
     */
    std::string to_string() const {
        std::ostringstream oss;
        oss << "Container State:\n";
        oss << "  Size: " << size << "\n";
        oss << "  Max Size: " << max_size << "\n";
        oss << "  Empty: " << (empty ? "true" : "false") << "\n";
        
        if (load_factor > 0.0f || max_load_factor > 0.0f) {
            oss << "  Load Factor: " << load_factor << "\n";
            oss << "  Max Load Factor: " << max_load_factor << "\n";
        }
        
        if (bucket_count > 0 || max_bucket_count > 0) {
            oss << "  Bucket Count: " << bucket_count << "\n";
            oss << "  Max Bucket Count: " << max_bucket_count << "\n";
        }
        
        return oss.str();
    }
    
    /**
     * 转换为JSON格式
     */
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"size\": " << size << ",";
        oss << "\"max_size\": " << max_size << ",";
        oss << "\"empty\": " << (empty ? "true" : "false") << ",