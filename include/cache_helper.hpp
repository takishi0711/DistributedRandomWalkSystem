#pragma once

#include <vector>
#include <unordered_map>
#include <shared_mutex>

#include "type.hpp"
#include "../config/param.hpp"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SimpleCache {

public :

    void init();

    // 隣接リスト情報内の index 存在確認
    // 存在したら next node ID を返す
    // 存在しなかったら INF を返す
    vertex_id_t getNextNodeID(const vertex_id_t& node_ID, const index_t& index_num);

    // index を登録
    // 
    void setIndex(const vertex_id_t& node_ID_u, const index_t& index_num, const vertex_id_t& node_ID_v);

    // debug 用
    // void printList();
    uint32_t getSize();

private : 

    std::vector<std::unordered_map<index_t, vertex_id_t>> cache_;
    std::atomic<uint64_t> cache_size_ = 0;

    std::shared_mutex* mtx_cache_;

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

inline void SimpleCache::init() {
    cache_.resize(VERTEX_SIZE);
    mtx_cache_ = new std::shared_mutex[VERTEX_SIZE];
}

inline vertex_id_t SimpleCache::getNextNodeID(const vertex_id_t& node_ID, const index_t& index_num) {
    {
        std::shared_lock<std::shared_mutex> lock(mtx_cache_[node_ID]);

        // if (!cache_.contains(node_ID)) return INF;
        if (!cache_[node_ID].contains(index_num)) return INF;

        return cache_[node_ID][index_num];
    }
}

inline void SimpleCache::setIndex(const vertex_id_t& node_ID_u, const index_t& index_num, const vertex_id_t& node_ID_v) {
    // if (cache_size_ >= MAX_CACHE_SIZE) return;
    if (cache_size_ + MY_EDGE_NUM >= MAX_CACHE_SIZE) {
        CHECK_RWER_FLAG = false;
        CACHE_GEN_FLAG = false;
        return;
    }

    bool exist_edge = false;
    {
        std::shared_lock<std::shared_mutex> lock(mtx_cache_[node_ID_u]);
        if (cache_[node_ID_u].contains(index_num)) exist_edge = true;
    }

    if (!exist_edge) {
        {
            std::lock_guard<std::shared_mutex> lock(mtx_cache_[node_ID_u]);

            if (!cache_[node_ID_u].contains(index_num)) {
                cache_[node_ID_u][index_num] = node_ID_v;
                cache_size_++;
                // if (cache_size_ >= MAX_CACHE_SIZE) {
                if (cache_size_ + MY_EDGE_NUM >= MAX_CACHE_SIZE) {
                    CHECK_RWER_FLAG = false;
                    CACHE_GEN_FLAG = false;
                }
            }
        }
    }
}

inline uint32_t SimpleCache::getSize() {
    return cache_size_;
}