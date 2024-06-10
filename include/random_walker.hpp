#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <unordered_set>
#include <iostream>
#include <bitset>
#include <cstring>

#include "../config/param.hpp"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// ver_id_ (8bit): 
// バージョン: 4bit, メッセージID: 4bit
// メッセージ ID について, 0 -> 生存した RWer, 1 -> 終了した RWer, 2 -> 複数の RWer が入っているパケット, 3 -> 実験開始の合図, 4 -> 実験終了の合図
// 
// flag_ (8bit): 
// 一歩前で通信が発生したか: 1bit, next_index に値が入っているか: 1bit, 全体を通して通信が発生したか: 1bit, あまり : 5bit
// 
// RWer_size_ (16bit):
// RWer 単体のメモリサイズ
//
// RWer_id_ (32bit):
//
// RWer_life_ (16bit):
// RWer の残り歩数
// 
// path_length_at_current_host_ (16bit):
// RWer の現在の同一ホスト内の経路長
//
// reserved_ (32bit):
// 予備
// 
// next_index_ (64bit):
// 通信が発生した時の次の遷移先 index
//
// path_ (64bit の可変長配列):
// 経路情報
// {HostID(48bit) + 同HostID内の経路長(15bit) + 通信が発生したか(1bit)}, {頂点(64bit), 次数(64bit), u->v の index(64bit), v->u の index(64bit), 頂点, 次数, ...}, {HostID(48bit) + 同HostID内の経路長(15bit) + 通信が発生したか(1bit)}, ...


struct RandomWalker {

public :

    // コンストラクタ
    RandomWalker();
    RandomWalker(const uint64_t& source_node, const uint64_t& node_degree, const uint32_t& RWer_id, const uint64_t& HostID, const uint32_t& RWer_life);
    RandomWalker(const char* message); // メッセージから RWer 復元
    RandomWalker(const uint32_t dummy); // ダミー RWer

    // メッセージIDを入れる
    void setMessageID(const uint8_t& id);

    // メッセージID取得
    uint8_t getMessageID();

    // RWer の ID を入手
    uint32_t getRWerID();

    // RWer のサイズを入手 (Byte 単位)
    uint32_t getRWerSize();

    // RWer が終了しているかどうか (true: 終了, false: 生存)
    bool isEnd();

     // 通信が発生したときに flag を入れる
    void setSendFlag(bool flag); 

    // 一歩前で通信が発生したか (true: した, false: してない)
    bool isSended();

    // 全体を通して通信が発生したか
    bool isSendedAll();

    // next_index_ に値が入っているかどうか
    bool isSetNextIndex();

    // next_index_ の値の flag を入れる
    void setNextIndexFlag(bool flag);

    // 通信が発生した時の次の遷移先 index を入力
    void setNextIndex(const uint64_t& index_num);

    // 通信が発生した時の次の遷移先 index を返す
    uint64_t getNextIndex();

    // 現在の Host index を入手
    uint64_t getCurrentHostIndex();

    // RWer_life を 1 減らす
    void decrementRWerLife();

    // path_ の次の index の位置 (頂点を入れる場所) を入手
    uint32_t getNextIndexOfPath();

    // 現在の頂点の path_ の index を入手
    uint32_t getCurrentIndexOfPath();

    // 現在の頂点 ID を入手
    uint64_t getCurrentNodeID();

    // 現在の頂点の次数を入力
    void setCurrentDegree(const uint64_t& node_degree);

    // 現在頂点の HostID を入手
    uint64_t getCurrentNodeHostID();

    // 一歩前の頂点の path_ の index を入手
    uint32_t getPrevIndexOfPath();

    // 一歩前の頂点 ID を入手
    uint64_t getPrevNodeID();

    // 一歩前の v -> u の index を登録
    void setPrevIndex(const uint64_t& index_num);

    // 起点サーバの HostIDを入手
    uint64_t getHostID();

    // RWer の現在頂点を更新
    void updateRWer(const uint64_t& next_node, const uint64_t& host_id, const uint64_t& node_degree, const uint64_t& index_uv, const uint64_t& index_vu);

    // path_ のメモリ確保のためのサイズを現在の RWer_size と RWer_life から算出
    uint16_t getRequiredPathSize();

    // RWer を終了させる
    void endRWer();

    // message に RWer のデータを書き込む
    void writeMessage(char* message);

    // path_ の {HostID(48bit) + 同HostID内の経路長(15bit) + 通信が発生したか(1bit)} から HostID と 同HostID内の経路長を抜き出す
    void getHostIDAndLengthInPath(const uint64_t& data, uint64_t& host_id, uint16_t& length);

    // 引数の path に path_ の情報を書き込む (各頂点にホストIDもつける), path_length に全経路長を書きこむ
    void getPath(uint16_t& path_length, std::vector<uint64_t>& path);

    // デバッグ用, RWer の出力
    void printRWer();


private :

    uint8_t ver_id_ = 0; 
    uint8_t flag_ = 0;
    uint16_t RWer_size_ = 0;
    uint32_t RWer_id_ = 0;    
    uint16_t RWer_life_ = 0; 
    uint16_t path_length_at_current_host_ = 0; 
    uint32_t reserved_ = 0; 
    uint64_t next_index_ = 0;
    std::vector<uint64_t> path_;

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

inline RandomWalker::RandomWalker() {
    setMessageID(ALIVE);
}

inline RandomWalker::RandomWalker(const uint64_t& source_node, const uint64_t& node_degree, const uint32_t& RWer_id, const uint64_t& HostID, const uint32_t& RWer_life) {
    if (RWer_life <= 0) {
        perror("initial life < 0");
        exit(1); // 異常終了
    }
    setMessageID(ALIVE);
    RWer_size_ = 8 + 8 + 8;
    path_length_at_current_host_ = 1;
    RWer_id_ = RWer_id;
    RWer_life_ = RWer_life;
    path_.resize(getRequiredPathSize());
    path_[0] = (HostID<<16) + (1<<1) + 1; RWer_size_ += 8; // HostID, 同HostID内の経路長入力 (終了した後最初のホストには送信するので, 送信フラグを入れておく)
    path_[1] = source_node; RWer_size_ += 8;
    path_[2] = node_degree; RWer_size_ += 8;
    path_[3] = 0; RWer_size_ += 8;
    path_[4] = 0; RWer_size_ += 8;

    decrementRWerLife();
}

inline RandomWalker::RandomWalker(const char* message) {
    int idx = 0;
    ver_id_ = *(uint8_t*)(message + idx); idx += 1;
    flag_ = *(uint8_t*)(message + idx); idx += 1;
    RWer_size_ = *(uint16_t*)(message + idx); idx += 2;
    RWer_id_ = *(uint32_t*)(message + idx); idx += 4;
    RWer_life_ = *(uint16_t*)(message + idx); idx += 2;
    path_length_at_current_host_ = *(uint16_t*)(message + idx); idx += 2;
    reserved_ = *(uint32_t*)(message + idx); idx += 4;
    next_index_ = *(uint64_t*)(message + idx); idx += 8;

    // debug
    // std::cout << "getRequiredPathSize() = " << getRequiredPathSize() << std::endl;

    path_.resize(getRequiredPathSize());
    int last_idx = getNextIndexOfPath() - 1;
    for (int i = 0; i <= last_idx; i++) {
        path_[i] = *(uint64_t*)(message + idx); idx += 8;
    }
}

inline RandomWalker::RandomWalker(const uint32_t dummy) {
    setMessageID(DUMMY);
}

inline void RandomWalker::setMessageID(const uint8_t& id) {
    ver_id_ &= ~MASK_MESSEGEID;
    ver_id_ |= id;
}

inline uint8_t RandomWalker::getMessageID() {
    return ver_id_ & MASK_MESSEGEID;
}

inline uint32_t RandomWalker::getRWerID() {
    return RWer_id_;
}

inline uint32_t RandomWalker::getRWerSize() {
    return RWer_size_;
}

inline bool RandomWalker::isEnd() {
    return getMessageID() == DEAD;
}

inline void RandomWalker::setSendFlag(bool flag) {
    flag_ &= ~(1<<7);
    flag_ |= (flag<<7);

    if (flag) {
        flag_ |= (1<<5);
    }
}

inline bool RandomWalker::isSended() {
    return (flag_>>7)&1;
}

inline bool RandomWalker::isSendedAll() {
    return (flag_>>5)&1;
}

inline bool RandomWalker::isSetNextIndex() {
    return (flag_>>6)&1;
}

inline void RandomWalker::setNextIndexFlag(bool flag) {
    flag_ &= ~(1<<6);
    flag_ |= (flag<<6);
}

inline void RandomWalker::setNextIndex(const uint64_t& index_num) {
    next_index_ = index_num;
    setNextIndexFlag(true);
}

inline uint64_t RandomWalker::getNextIndex() {
    if (!isSetNextIndex()) {
        perror("getNextIndex: no value");
        exit(1);
    }
    setNextIndexFlag(false);
    return next_index_;
}

inline uint64_t RandomWalker::getCurrentHostIndex() {
    return getCurrentIndexOfPath() - (4*(path_length_at_current_host_ - 1) + 1);
}

inline void RandomWalker::decrementRWerLife() {
    RWer_life_--;
    if (RWer_life_ == 0) endRWer();
}

inline uint32_t RandomWalker::getNextIndexOfPath() {
    // RWer_size_ から逆算
    return (RWer_size_ - 8 - 8 - 8) / 8;
}

inline uint32_t RandomWalker::getCurrentIndexOfPath() {
    return getNextIndexOfPath() - 4;
}

inline uint64_t RandomWalker::getCurrentNodeID() {
    return path_[getCurrentIndexOfPath()];
}

inline void RandomWalker::setCurrentDegree(const uint64_t& node_degree) {
    path_[getCurrentIndexOfPath() + 1] = node_degree;
}

inline uint64_t RandomWalker::getCurrentNodeHostID() {
    return path_[getCurrentHostIndex()]>>16;
}

inline uint32_t RandomWalker::getPrevIndexOfPath() {
    uint32_t prev_index;
    if (path_length_at_current_host_ == 1) prev_index = getNextIndexOfPath() - (4 + 1 + 4);
    else prev_index = getNextIndexOfPath() - (4 + 4);

    if (prev_index < 0) {
        std::cout << "no prev" << std::endl;
    }

    return prev_index;
}

inline uint64_t RandomWalker::getPrevNodeID() {
    int idx = getPrevIndexOfPath();
    if (idx < 0) return INF;
    return path_[idx];
}

inline void RandomWalker::setPrevIndex(const uint64_t& index_num) {
    uint64_t current_index = getCurrentIndexOfPath();

    path_[current_index + 3] = index_num;
}

inline uint64_t RandomWalker::getHostID() {
    return (path_[0]>>16);
}

inline void RandomWalker::updateRWer(const uint64_t& next_node, const uint64_t& host_id, const uint64_t& node_degree, const uint64_t& index_uv, const uint64_t& index_vu) {
    uint32_t start_index = getNextIndexOfPath();

    // debug
    // std::cout << "getNextIndexOfPath() = " << getNextIndexOfPath() << std::endl;

    if (isSended()) { // 送信が発生してたら path_ 上の現在のホスト情報の送信フラグを立てる
        path_[getCurrentHostIndex()] |= 1; // 送信フラグを立てる
        setSendFlag(false);
    } 

    if (getCurrentNodeHostID() != host_id) { // 次の頂点のホスト ID が現在と異なっていたら path_ 上にホスト情報を追加
        path_[start_index++] = (host_id<<16); RWer_size_ += 8; // HostID 入力
        path_length_at_current_host_ = 0;
    }
    

    // debug
    // std::cout << "getCurrentNodeHostID() = " << getCurrentNodeHostID() << std::endl;

    path_[start_index++] = next_node; RWer_size_ += 8;
    path_[start_index++] = node_degree; RWer_size_ += 8;
    path_[start_index++] = index_uv; RWer_size_ += 8;
    path_[start_index++] = index_vu; RWer_size_ += 8;
    
    path_length_at_current_host_++;
    path_[getCurrentHostIndex()] += (1<<1);

    decrementRWerLife();
}

inline uint16_t RandomWalker::getRequiredPathSize() {
    return (RWer_size_ - 8 - 8 - 8)/8 + RWer_life_*5;
}

inline void RandomWalker::endRWer() {
    setMessageID(DEAD);
}

inline void RandomWalker::writeMessage(char* message) {
    int idx = 0;
    memcpy(message + idx, &ver_id_, sizeof(uint8_t)); idx += sizeof(uint8_t);
    memcpy(message + idx, &flag_, sizeof(uint8_t)); idx += sizeof(uint8_t);
    memcpy(message + idx, &RWer_size_, sizeof(uint16_t)); idx += sizeof(uint16_t);
    memcpy(message + idx, &RWer_id_, sizeof(uint32_t)); idx += sizeof(uint32_t);
    memcpy(message + idx, &RWer_life_, sizeof(uint16_t)); idx += sizeof(uint16_t);
    memcpy(message + idx, &path_length_at_current_host_, sizeof(uint16_t)); idx += sizeof(uint16_t);
    memcpy(message + idx, &reserved_, sizeof(uint32_t)); idx += sizeof(uint32_t);
    memcpy(message + idx, &next_index_, sizeof(uint64_t)); idx += sizeof(uint64_t);
    
    int i = 0;
    while (idx < RWer_size_) {
        memcpy(message + idx, &path_[i], sizeof(uint64_t)); idx += sizeof(uint64_t);
        i++;
    }
}

inline void RandomWalker::getHostIDAndLengthInPath(const uint64_t& data, uint64_t& host_id, uint16_t& length) {
    host_id = data>>16;
    length = (data & ~((host_id)<<16))>>1;
}

inline void RandomWalker::getPath(uint16_t& path_length, std::vector<uint64_t>& path) {
    // path: (頂点, ホストID, 次数, indexuv, indexvu), (), (), ... のようにする

    uint16_t path__length = (RWer_size_ - 8 - 8 - 8) / 8; // path_ の長さ
    int idx = 0;
    while (idx < path__length) {
        // HostID, 同一ホスト内の歩長を抜き出す
        uint64_t host_id; uint16_t length;
        getHostIDAndLengthInPath(path_[idx++], host_id, length);
        path_length += length;
        for (int i = 0; i < length; i++) {
            path.push_back(path_[idx++]); // 頂点
            path.push_back(host_id); // ホストID
            path.push_back(path_[idx++]); // 次数
            path.push_back(path_[idx++]); // indexuv
            path.push_back(path_[idx++]); // indexvu
        } 
    }
}

inline void RandomWalker::printRWer() {
    std::cout << "ver_id_: " << std::bitset<8>(ver_id_) << std::endl;
    std::cout << "flag_: " << std::bitset<8>(flag_) << std::endl;
    std::cout << "RWer_size_: " << RWer_size_ << std::endl;
    std::cout << "RWer_id_: " << RWer_id_ << std::endl;
    std::cout << "RWer_life_: " << RWer_life_ << std::endl;
    std::cout << "path_length_at_current_host_: " << path_length_at_current_host_ << std::endl;
    std::cout << "reserved_: " << reserved_ << std::endl;
    std::cout << "next_index_: " << next_index_ << std::endl;
    uint16_t path__length = (RWer_size_ - 8 - 8 - 8) / 8;
    std::cout << "path__length: " << path__length << std::endl;
    int idx = 0;
    while (idx < path__length) {
        // HostID, 同一ホスト内の歩長を抜き出す
        uint64_t host_id; uint16_t length;
        uint8_t send_flag = path_[idx]&1;
        getHostIDAndLengthInPath(path_[idx++], host_id, length);
        printf("{%ld, %d, %d}, ", host_id, length, send_flag);
        for (int i = 0; i < length; i++) {
            // printf("(%ld, %ld, %ld, %ld), ", path_[idx++], path_[idx++], path_[idx++], path_[idx++]);
            printf("(%ld, %ld, %ld, %ld), ", path_[idx+0], path_[idx+1], path_[idx+2], path_[idx+3]);
            idx += 4;
        }
        std::cout << std::endl;
    }

    for (int i = 0; i < 3; i++) std::cout << std::endl;
}