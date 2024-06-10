#pragma once

#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <unordered_map>
#include <vector>
#include <utility>

#include "random_walker.hpp"
#include "graph.hpp"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <typename T>
struct MessageQueue {

public :

    void push(std::unique_ptr<T>&& message) {
        { // 排他制御
            std::lock_guard<std::mutex> lk(mtx_message_queue_);

            bool queue_empty = message_queue_.empty();

            message_queue_.push(std::move(message));

            if (queue_empty) { // 空キューでなくなった通知
                cv_message_queue_.notify_all();
            }

        }
    }

    void push(std::vector<std::unique_ptr<RandomWalker>>& RWer_ptr_vec) {
        { // 排他制御
            std::lock_guard<std::mutex> lk(mtx_message_queue_);

            bool queue_empty = message_queue_.empty();

            uint32_t vec_size = RWer_ptr_vec.size();
            for (int i = 0; i < vec_size; i++) {
                message_queue_.push(std::move(RWer_ptr_vec[i]));
            }

            if (queue_empty) { // 空キューでなくなった通知
                cv_message_queue_.notify_all();
            }

        }
    }

    // message_queue_ から message をまとめて取り出す
    // vector に格納
    // 入れた数を返す
    uint32_t pop(std::vector<std::unique_ptr<T>>& ptr_vec) {
        { // 排他制御
            std::unique_lock<std::mutex> lk(mtx_message_queue_);

            // RWer_Queue が空じゃなくなるまで待機
            cv_message_queue_.wait(lk, [&]{ return !message_queue_.empty(); });

            uint32_t vec_size = message_queue_.size();
            ptr_vec.reserve(vec_size);

            while (message_queue_.size()) {
                std::unique_ptr<T> ptr = std::move(message_queue_.front());
                message_queue_.pop();
                ptr_vec.push_back(std::move(ptr));
            }

            return vec_size;
        }
    }

    // message_queue_ のサイズを入手
    uint32_t getSize() {
        { // 排他制御
            std::lock_guard<std::mutex> lk(mtx_message_queue_);

            return message_queue_.size();
        }        
    }

private :

    std::queue<std::unique_ptr<T>> message_queue_;
    std::mutex mtx_message_queue_;
    std::condition_variable cv_message_queue_; 

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////