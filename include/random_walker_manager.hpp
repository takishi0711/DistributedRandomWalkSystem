#pragma once

#include <chrono>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <atomic>

#include "../config/param.hpp"
#include "type.hpp"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class RandomWalkerManager {

public :
    
    // RWer の総数を入力し, 配列を new
    void init(const walker_id_t& RWer_all);

    // RWer 生成時間の記録
    void setStartTime(const walker_id_t& RWer_id);

    // RWer 終了時間の記録
    void setEndTime(const walker_id_t& RWer_id);

    // RWer の歩数を入力
    void setRWerLife(const walker_id_t& RWer_id, const uint16_t& life);

    // node_id を入力
    void setNodeId(const walker_id_t& RWer_id, const vertex_id_t& node_id);

    // RWer 終了数の入手
    walker_id_t getEndcnt();

    // 実行時間 (s) を入手 (一番遅い RWer の終了時間 - 一番早く生成された RWer の生成時間)
    double getExecutionTime();

private :

    walker_id_t RWer_all_num_ = 0; // RWer の総数
    bool* start_flag_per_RWer_id_ = nullptr; // RWer_id に対する終了判定
    bool* end_flag_per_RWer_id_ = nullptr; // RWer_id に対する終了判定
    std::chrono::system_clock::time_point* start_time_per_RWer_id_ = nullptr; // RWer_id に対する開始時刻
    std::chrono::system_clock::time_point* end_time_per_RWer_id_ = nullptr; // RWer_id に対する終了時刻
    uint16_t* RWer_life_per_RWer_id_ = nullptr; // RWer_id に対する設定歩数
    vertex_id_t* node_id_per_RWer_id_ = nullptr; // RWer_id に対する node_id

    walker_id_t start_count_ = 0;
    std::atomic<walker_id_t> end_count_ = 0;

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

inline void RandomWalkerManager::init(const walker_id_t& RWer_all) {
    RWer_all_num_ = RWer_all;
    start_flag_per_RWer_id_ = new bool[RWer_all];
    end_flag_per_RWer_id_ = new bool[RWer_all];
    start_time_per_RWer_id_ = new std::chrono::system_clock::time_point[RWer_all];
    end_time_per_RWer_id_ = new std::chrono::system_clock::time_point[RWer_all];
    RWer_life_per_RWer_id_ = new uint16_t[RWer_all];
    node_id_per_RWer_id_ = new uint64_t[RWer_all];
}

inline void RandomWalkerManager::setStartTime(const walker_id_t& RWer_id) {
    start_flag_per_RWer_id_[RWer_id] = true;
    start_time_per_RWer_id_[RWer_id] = std::chrono::system_clock::now();
}

inline void RandomWalkerManager::setEndTime(const walker_id_t& RWer_id) {
    // debug
    // std::cout << "SetEndTime" << std::endl;

    if (end_flag_per_RWer_id_[RWer_id] == true) {
        return;
    }

    end_flag_per_RWer_id_[RWer_id] = true;
    end_time_per_RWer_id_[RWer_id] = std::chrono::system_clock::now();

    // addEndCount();
    end_count_++;
}

inline void RandomWalkerManager::setRWerLife(const walker_id_t& RWer_id, const uint16_t& life) {
    RWer_life_per_RWer_id_[RWer_id] = life;
}

inline void RandomWalkerManager::setNodeId(const walker_id_t& RWer_id, const uint64_t& node_id) {
    node_id_per_RWer_id_[RWer_id] = node_id;
}

inline walker_id_t RandomWalkerManager::getEndcnt() {
    return end_count_;
}

inline double RandomWalkerManager::getExecutionTime() {
    std::chrono::system_clock::time_point min_start_time = start_time_per_RWer_id_[0];
    std::chrono::system_clock::time_point max_end_time = start_time_per_RWer_id_[0];
    for (int32_t id = 0; id < RWer_all_num_; id++) {
        min_start_time = std::min(min_start_time, start_time_per_RWer_id_[id]);
        if (end_flag_per_RWer_id_[id]) { // 終了しているもののみ
            max_end_time = std::max(max_end_time, end_time_per_RWer_id_[id]);
        }
    }
    double execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(max_end_time - min_start_time).count();
    execution_time /= 1000;
    return execution_time;
}