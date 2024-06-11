#pragma once

#include <random>

#include "util.hpp"
#include "../config/param.hpp"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class RandomWalkConfig {

public :

    // RW の実行回数を設定
    void setNumberOfRWExecution(const uint32_t& RW_num);

    // RW の実行回数を入手
    uint32_t getNumberOfRWExecution();

    // RW の終了確率を入手
    double getAlpha();

    // α の値から
    uint16_t getRWerLife(StdRandNumGenerator& gen);

private :
    uint32_t number_of_RW_execution_ = 10000; // RW の実行回数
    double alpha_ = ALPHA; // RW の終了確率

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

inline void RandomWalkConfig::setNumberOfRWExecution(const uint32_t& RW_num) {
    number_of_RW_execution_ = RW_num;
}

inline uint32_t RandomWalkConfig::getNumberOfRWExecution() {
    return number_of_RW_execution_;
}

inline double RandomWalkConfig::getAlpha() {
    return alpha_;
}

inline uint16_t RandomWalkConfig::getRWerLife(StdRandNumGenerator& gen) {
    uint16_t life = 1;
    while (gen.gen_float(1.0) > alpha_) life++;
    return life;
}

