#pragma once

#include <mutex>
#include <condition_variable>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class StartFlag {

public :

    // ready への書き込み
    void writeReady(bool flag);

    // ready の読み込み
    bool readReady();

    // ready が true になるまで (開始の合図が来るまで) ロックする関数
    void lockWhileFalse();

private :

    bool ready_ = false; // 実験開始フラグ
    std::mutex mtx_ready_; // ready 用の mutex
    std::condition_variable cv_ready; // ready 用の condition variable

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


inline void StartFlag::writeReady(bool flag) {
    {
        std::unique_lock<std::mutex> uniq_lk(mtx_ready_);
        ready_ = flag;
        cv_ready.notify_all();
    }
}


inline bool StartFlag::readReady() {
    {
        std::unique_lock<std::mutex> uniq_lk(mtx_ready_);
        return ready_;
    }
}


inline void StartFlag::lockWhileFalse() {
    {
        std::unique_lock<std::mutex> uniq_lk(mtx_ready_);
        cv_ready.wait(uniq_lk, [&]{ return ready_;});
        ready_ = false;
    }
}