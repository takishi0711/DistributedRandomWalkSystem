#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <omp.h>
#include <chrono>
#include <thread>
#include <memory>
#include <utility>
#include <mutex>
#include <atomic>
#include <unordered_map>

#include "type.hpp"
#include "graph.hpp"
#include "cache.hpp"
#include "message_queue.hpp"
#include "random_walker.hpp"
#include "util.hpp"
#include "start_flag.hpp"
#include "random_walk_config.hpp"
#include "random_walker_manager.hpp"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class RandomWalkSystemWorker {

public :

    // コンストラクタ
    RandomWalkSystemWorker(const std::string& dir_path);

    // thread を開始させる関数
    void start();

    // RWer 生成 & 処理をする関数
    void generateRWerForMain();

    // cache 補充用 RWer 生成
    void generateRWerForCache();

    // RW を実行する関数
    void executeRandomWalk(std::unique_ptr<RandomWalker>&& RWer_ptr, StdRandNumGenerator& gen);

    // executeRandomWalk で終了した RWer を処理する関数
    void endRandomWalk(std::unique_ptr<RandomWalker>&& RWer_ptr);

    // 終了した RWer について, 経路情報からグラフデータにキャッシュを登録する関数
    void checkRWer(std::unique_ptr<RandomWalker>&& RWer_ptr);

    // メッセージ処理用の関数
    void procMessage(const uint16_t& proc_id);

    // send_queue から RWer を取ってきて他サーバへ送信する関数 (スレッド数固定)
    void sendMessage();

    // 他サーバからメッセージを受信し, message_queue に push する関数 (ポート番号毎)
    void receiveMessage(const uint16_t& port_num);

    // IPv4 サーバソケットを生成 (UDP)
    int createUdpServerSocket(const uint16_t& port_num);

    // IPv4 サーバソケットを生成 (TCP)
    int createTcpServerSocket(const uint16_t& port_num);

    // 実験結果を start_manager に送信する関数
    void sendToStartManager();

private :

    std::string hostname_; // 自サーバのホスト名
    host_id_t hostip_; // 自サーバの IP アドレス
    host_id_t hostid_;
    std::string hostip_str_; // IP アドレスの文字列
    std::vector<host_id_t> worker_ip_all_;
    Graph graph_; // グラフデータ
    Cache cache_; // 他サーバのグラフ情報
    MessageQueue<RandomWalker>* RWer_queue_; // ポート番号毎の receive キュー
    MessageQueue<RandomWalker>* send_queue_; // 送信先毎の send キュー
    StartFlag start_flag_; // 実験開始の合図に関する情報
    StartFlag start_cache_flag_; // cache 実行開始の合図に関する情報
    RandomWalkConfig RW_config_; // Random Walk 実行関連の設定
    RandomWalkerManager RW_manager_; // RWer に関する情報
    host_id_t startmanagerip_; // StartManager の IP アドレス

    // 送信スレッドの送信先決定用
    host_id_t id_num_ = 0;
    std::mutex mtx_id_num_;
    std::atomic_bool* watching_queue_flag_;

    // 再送制御用 
    std::vector<std::thread> re_send_threads_;
    uint32_t re_send_count = 0;

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

inline RandomWalkSystemWorker::RandomWalkSystemWorker(const std::string& dir_path) {
    // 自サーバ のホスト名
    char hostname_c[128]; // ホスト名
    gethostname(hostname_c, sizeof(hostname_c)); // ホスト名を取得
    hostname_ = hostname_c; // char* から string へ

    // 自サーバ の IP アドレス (NIC 依存)
    const char* ipname;
    {
        std::unordered_map<std::string, std::string> mp;
        std::ifstream reading_file;
        reading_file.open("../config/hostname_nic.txt", std::ios::in);
        std::string reading_line_buffer;
        while (std::getline(reading_file, reading_line_buffer)) { // 1 行ずつ読み取り
            std::vector<std::string> words; // [hostname, nic]
            std::stringstream sstream(reading_line_buffer);
            std::string word;
            while (std::getline(sstream, word, ' ')) { // 空白区切りで word を取り出す
                words.push_back(word);
            }
            mp[words[0]] = words[1];
        }  
        ipname = mp[hostname_].c_str();
        // debug
        std::cout << "hostname: " << hostname_ << ", nic: " << mp[hostname_] << std::endl;
    }

    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    ifr.ifr_addr.sa_family = AF_INET; // IPv4 の IP アドレスを取得したい
    strncpy(ifr.ifr_name, ipname, IFNAMSIZ-1); // ipname の IP アドレスを取得したい
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);
    hostip_ = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
    hostip_str_ = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    // debug
    std::cout << "hostip_str_: " << hostip_str_ << std::endl;

    // worker の IP アドレス情報を入手
    {
        std::ifstream reading_file;
        reading_file.open("../config/server.txt", std::ios::in);
        std::string reading_line_buffer;
        while (std::getline(reading_file, reading_line_buffer)) {
            worker_ip_all_.emplace_back(inet_addr(reading_line_buffer.c_str()));
        }
        for (int i = 0; i < worker_ip_all_.size(); i++) {
            if (hostip_ == worker_ip_all_[i]) hostid_ = i;
        }
        // debug
        std::cout << hostid_ << std::endl;
    }

    // グラフファイル読み込み
    graph_.init(dir_path, hostip_str_, hostid_);

    // キャッシュの初期化
    cache_.init();

    // 受信キューの初期化
    RWer_queue_ = new MessageQueue<RandomWalker>[PROC_MESSAGE_THREAD_NUM];

    // 送信キューの初期化
    watching_queue_flag_ = new std::atomic<bool>[SEND_QUEUE_NUM];
    for (int i = 0; i < SEND_QUEUE_NUM; i++) {
        watching_queue_flag_[i] = false;
    }
    send_queue_ = new MessageQueue<RandomWalker>[SEND_QUEUE_NUM];

    // 全てのスレッドを開始させる 
    start();
}

inline void RandomWalkSystemWorker::start() {
    std::thread thread_generateRWer(&RandomWalkSystemWorker::generateRWerForMain, this);
    std::thread thread_generateRWerCache(&RandomWalkSystemWorker::generateRWerForCache, this);

    std::vector<std::thread> threads_sendMessage;
    for (int i = 0; i < SEND_QUEUE_NUM; i++) {
        if (i == hostid_) continue;
        threads_sendMessage.emplace_back(std::thread(&RandomWalkSystemWorker::sendMessage, this));
    }

    std::vector<std::thread> threads_receiveMessage;
    for (int i = 0; i < RECV_PORT; i++) {
        threads_receiveMessage.emplace_back(std::thread(&RandomWalkSystemWorker::receiveMessage, this, 10000+i));
    }

    // プログラムを終了させないようにする
    thread_generateRWer.join();
}

inline void RandomWalkSystemWorker::generateRWerForMain() {
    std::cout << "generateRWerForMain" << std::endl;

    // スレッドごとの乱数生成器
    StdRandNumGenerator* randgen = new StdRandNumGenerator[GENERATE_RWER_THREAD_NUM];

    while (1) {
        // 開始通知を受けるまでロック
        start_flag_.lockWhileFalse();

        // debug
        std::cout << "start" << std::endl;

        uint64_t number_of_RW_execution = RW_config_.getNumberOfRWExecution();
        uint64_t number_of_my_vertices = graph_.getMyVerticesNum();
        std::vector<vertex_id_t> my_vertices = graph_.getMyVertices();
        walker_id_t RWer_num_all = number_of_my_vertices * number_of_RW_execution;

        RW_manager_.init(RWer_num_all);

        // debug
        std::cout << "GENERATE_RWER_THREAD_NUM: " << GENERATE_RWER_THREAD_NUM << std::endl;

        Timer timer;
        #pragma omp parallel num_threads(GENERATE_RWER_THREAD_NUM)
        {
            worker_id_t worker_id = omp_get_thread_num();
            StdRandNumGenerator gen = randgen[worker_id];
            walker_id_t RWer_id = worker_id;
            bool sleep_flag = false;

            while (1) {
                vertex_id_t node_id = my_vertices[RWer_id % number_of_my_vertices];

                // 歩数を生成
                uint16_t life = RW_config_.getRWerLife(gen);

                // RWer を生成
                std::unique_ptr<RandomWalker> RWer_ptr(new RandomWalker(node_id, graph_.getDegree(node_id), RWer_id, hostid_, life));

                // 生成時刻を記録
                RW_manager_.setStartTime(RWer_id);

                // 歩数を記録
                RW_manager_.setRWerLife(RWer_id, life);

                // node_id を記録
                RW_manager_.setNodeId(RWer_id, node_id);

                // RW を実行 
                executeRandomWalk(std::move(RWer_ptr), gen);

                RWer_id += GENERATE_RWER_THREAD_NUM;
                if (RWer_id >= RWer_num_all) break;
            }
        }

        std::cout << "generate end: " << timer.duration() << std::endl;

        PROC_MESSAGE_FLAG = true;
        std::vector<std::thread> threads_procMessage;
        for (int i = 0; i < PROC_MESSAGE_THREAD_NUM; i++) {
            threads_procMessage.emplace_back(std::thread(&RandomWalkSystemWorker::procMessage, this, i));
        }

        threads_procMessage[0].join();
    }

    delete[] randgen;
}

inline void RandomWalkSystemWorker::generateRWerForCache() {
    std::cout << "generateRWerForCache" << std::endl;

    uint64_t number_of_my_vertices = graph_.getMyVerticesNum();
    std::vector<vertex_id_t> my_vertices = graph_.getMyVertices();

    // procMessage スレッドを生成
    PROC_MESSAGE_FLAG = true;
    std::vector<std::thread> threads_procMessage;
    for (int i = 0; i < PROC_MESSAGE_CACHE_THREAD_NUM; i++) {
        threads_procMessage.emplace_back(std::thread(&RandomWalkSystemWorker::procMessage, this, i));
    }

    start_cache_flag_.lockWhileFalse();

    Timer timer;
    uint32_t RWer_id_all;
    #pragma omp parallel num_threads(GENERATE_RWER_CACHE_THREAD_NUM)
    {
        worker_id_t worker_id = omp_get_thread_num();
        StdRandNumGenerator gen;
        walker_id_t RWer_id = worker_id;
        walker_id_t sleep_threashold = RW_STEP;

        while (CACHE_GEN_FLAG) {

            vertex_id_t node_id = my_vertices[RWer_id % number_of_my_vertices];

            // 歩数を生成
            uint16_t life = RW_config_.getRWerLife(gen);

            // RWer を生成
            std::unique_ptr<RandomWalker> RWer_ptr(new RandomWalker(node_id, graph_.getDegree(node_id), RWer_id, hostid_, life));

            // RW を実行 
            executeRandomWalk(std::move(RWer_ptr), gen);

            RWer_id += GENERATE_RWER_CACHE_THREAD_NUM;
            if (RWer_id >= MAX_RWER_NUM_FOR_CACHE) break;
            if (RWer_id >= sleep_threashold) {
                // debug
                std::cout << "RWer_id: " << RWer_id << ", sleep" << std::endl;

                std::this_thread::sleep_for(std::chrono::seconds(GENERATE_SLEEP_TIME));

                sleep_threashold += RW_STEP;

                // debug
                std::cout << "start" << std::endl;
                std::cout << "cache count: " << cache_.getEdgeCount() << std::endl;
            }
        }

        // debug 
        std::cout << RWer_id << std::endl;
        RWer_id_all = RWer_id;
    }

    double execution_time = timer.duration();
    std::cout << "ex_time: " << execution_time << ", cache_size: " << cache_.getEdgeCount() << ", RWer_id_all: " << RWer_id_all << std::endl;

    StdRandNumGenerator gen;
    std::this_thread::sleep_for(std::chrono::seconds(gen.gen(5)));

    // startmanager に結果送信
    {
        // ソケットの生成
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) { // エラー処理
            perror("socket");
            exit(1); // 異常終了
        }
        std::cout << sockfd << std::endl;

        // アドレスの生成
        struct sockaddr_in addr; // 接続先の情報用の構造体(ipv4)
        memset(&addr, 0, sizeof(struct sockaddr_in)); // memsetで初期化
        addr.sin_family = AF_INET; // アドレスファミリ(ipv4)
        addr.sin_port = htons(9999); // ポート番号, htons()関数は16bitホストバイトオーダーをネットワークバイトオーダーに変換
        addr.sin_addr.s_addr = startmanagerip_; // IPアドレス, inet_addr()関数はアドレスの翻訳

        // ソケット接続要求
        connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)); // ソケット, アドレスポインタ, アドレスサイズ
        std::cout << "connect" << std::endl;

        // データ送信 (hostip: 4B, execution_time: 8B)
        char message[MESSAGE_MAX_LENGTH_SEND];
        int idx = 0;
        memcpy(message + idx, &hostip_, sizeof(uint32_t)); idx += sizeof(uint32_t);
        // memcpy(message + idx, &execution_time, sizeof(double)); idx += sizeof(double);
        memcpy(message + idx, &RWer_id_all, sizeof(uint32_t)); idx += sizeof(uint32_t);
        send(sockfd, message, sizeof(message), 0); // 送信
        std::cout << "send" << std::endl;

        // ソケットクローズ
        close(sockfd); 
    }

    // 全てのサーバで終了した確認
    {
        int sockfd = createTcpServerSocket(9999); // サーバソケットを生成 (TCP)

        struct sockaddr_in get_addr; // 接続相手のソケットアドレス
        socklen_t len = sizeof(struct sockaddr_in); // 接続相手のアドレスサイズ
        int connect = accept(sockfd, (struct sockaddr *)&get_addr, &len); // 接続待ちソケット, 接続相手のソケットアドレスポインタ, 接続相手のアドレスサイズ
        if (connect < 0) { // エラー処理
            perror("accept");
            exit(1); // 異常終了
        }       

        char message[1024]; // 受信バッファ
        memset(message, 0, sizeof(message)); // 受信バッファ初期化
        recv(connect, message, sizeof(message), 0); // 受信 

        close(sockfd);
    }

    // procMessageスレッドを終了させる
    PROC_MESSAGE_FLAG = false;
    for (int i = 0; i < PROC_MESSAGE_CACHE_THREAD_NUM; i++) {
        std::unique_ptr<RandomWalker> RWer_ptr(new RandomWalker(DUMMY));
        RWer_queue_[i].push(std::move(RWer_ptr));
        threads_procMessage[i].join();
    }
    std::cout << "PROC_MESSAGE join !" << std::endl;
}

inline void RandomWalkSystemWorker::executeRandomWalk(std::unique_ptr<RandomWalker>&& RWer_ptr, StdRandNumGenerator& gen) {

    while (1) {

        vertex_id_t current_node = RWer_ptr->getCurrentNodeID(); // 現在頂点

        if (graph_.hasVertex(current_node)) { // 元グラフのデータを参照して RW

            index_t degree = graph_.getDegree(current_node);

            // 現在頂点の次数情報を RWer に入力
            RWer_ptr->setCurrentDegree(degree);

            // current node -> prev node の index を登録
            vertex_id_t prev_node = RWer_ptr->getPrevNodeID();
            if (prev_node != INF) RWer_ptr->setPrevIndex(graph_.indexOfUV(current_node, prev_node));

            // RW を一歩進める
            if (RWer_ptr->isSended() == true && RWer_ptr->isSetNextIndex() == true) { // 他のサーバから送られてきた RWer

                index_t next_index = RWer_ptr->getNextIndex();
                vertex_id_t next_node = graph_.getNextNodeID(current_node, next_index, gen);

                RWer_ptr->updateRWer(next_node, graph_.getHostId(next_node), INF, next_index, INF);

            } else if (RWer_ptr->isEnd() || degree == 0) { // 寿命切れ もしくは次数 0 なら終了
                
                // 終了した RWer の処理 
                endRandomWalk(std::move(RWer_ptr));

                break;

            } else { // ランダムな隣接ノードへ遷移

                index_t next_index = gen.gen(degree);
                vertex_id_t next_node = graph_.getNextNodeID(current_node, next_index, gen);

                RWer_ptr->updateRWer(next_node, graph_.getHostId(next_node), 0, next_index, INF);
            }

        } else { // キャッシュデータを参照して RW

            // 現在頂点の次数情報があるか確認
            if (!cache_.hasDegree(current_node)) { // 次数情報がない (元グラフの他サーバ隣接ノードの初期状態)

                RWer_ptr->setSendFlag(true);
                send_queue_[graph_.getHostId(current_node)].push(std::move(RWer_ptr));

                break;
            }

            // 次数をキャッシュからコピーして取ってくる
            index_t degree = cache_.getDegree(current_node);

            // 現在頂点の次数情報を RWer に入力
            RWer_ptr->setCurrentDegree(degree);

            // RW を一歩進める
            if (RWer_ptr->isEnd() || degree == 0) { // 寿命切れ もしくは次数 0 なら終了

                // 終了した RWer の処理
                endRandomWalk(std::move(RWer_ptr));

                break;

            } else { // ランダムな隣接ノードへ遷移

                // 0 <= rand_idx < degree をランダム生成
                index_t rand_idx = gen.gen(degree);

                vertex_id_t next_node = cache_.getNextNodeID(current_node, rand_idx);

                if (next_node == INF) { // index が存在してなかった場合は index とともに送信

                    RWer_ptr->setNextIndex(rand_idx);
                    RWer_ptr->setSendFlag(true);
                    send_queue_[cache_.getHostId(current_node)].push(std::move(RWer_ptr));

                    break;

                }

                if (graph_.hasVertex(next_node)) RWer_ptr->updateRWer(next_node, graph_.getHostId(next_node), INF, rand_idx, INF);
                else RWer_ptr->updateRWer(next_node, cache_.getHostId(next_node), INF, rand_idx, INF);
            }
            
        }
    }
}

inline void RandomWalkSystemWorker::endRandomWalk(std::unique_ptr<RandomWalker>&& RWer_ptr) {
    // RWer の message_id に DEAD_SEND フラグを入れる
    RWer_ptr->setMessageID(DEAD_SEND);

    if (RWer_ptr->getHostID() == hostid_) {
        if (CHECK_RWER_FLAG && RWer_ptr->isSendedAll()) checkRWer(std::move(RWer_ptr));
        else if (MAIN_EX) RW_manager_.setEndTime(RWer_ptr->getRWerID());
    } else {
        send_queue_[RWer_ptr->getHostID()].push(std::move(RWer_ptr));
    }
}

inline void RandomWalkSystemWorker::checkRWer(std::unique_ptr<RandomWalker>&& RWer_ptr) {
    // debug
    // std::cout << "checkRWer" << std::endl;

    // RWer の起点サーバがここだったら終了時間記録
    if (RWer_ptr->getHostID() == hostid_) {
        // std::cout << "endatstartserver" << std::endl;
        if (MAIN_EX) RW_manager_.setEndTime(RWer_ptr->getRWerID());
    }

    // debug 
    // std::cout << "not host server of RWer" << std::endl;

    // RWer の経路情報をキャッシュに登録
    cache_.addRWer(std::move(RWer_ptr), graph_);
}

inline void RandomWalkSystemWorker::procMessage(const uint16_t& proc_id) {
    std::cout << "procMessage: " << proc_id << ", " << RWer_queue_[proc_id].getSize() << std::endl;

    StdRandNumGenerator randgen;

    while (PROC_MESSAGE_FLAG) {
        // メッセージキューからメッセージを取得
        std::vector<std::unique_ptr<RandomWalker>> RWer_ptr_vec;
        uint32_t vec_size = RWer_queue_[proc_id].pop(RWer_ptr_vec);

        // debug
        // RWer.printRWer();
        // std::cout << "vec_size: " << vec_size << std::endl;

        for (int i = 0; i < vec_size; i++) {
            uint8_t message_id = RWer_ptr_vec[i]->getMessageID();
            if (message_id == DEAD_SEND) { // 終了して送られてきた RWer の処理

                if (CHECK_RWER_FLAG) checkRWer(std::move(RWer_ptr_vec[i]));
                else if (MAIN_EX) RW_manager_.setEndTime(RWer_ptr_vec[i]->getRWerID());

            } else if (message_id == DUMMY) {

                continue;

            } else { // まだ生存している RWer の処理

                // RW を実行
                executeRandomWalk(std::move(RWer_ptr_vec[i]), randgen);

            }            
        }

    }  

}

void RandomWalkSystemWorker::sendMessage() {
    std::cout << "sendMessage" << std::endl;

    // ソケットの生成
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    // debug 
    // std::cout << sockfd << std::endl;
    if (sockfd < 0) { // エラー処理
        perror("socket");
        exit(1); // 異常終了
    } 

    StdRandNumGenerator gen;
    char message[MESSAGE_MAX_LENGTH_SEND];
    memset(message, 0, MESSAGE_MAX_LENGTH_SEND);
    uint8_t ver_id = RWERS;
    uint16_t RWer_count = 0;
    uint32_t now_length = 0;

    // アドレスの生成
    struct sockaddr_in addr; // 接続先の情報用の構造体(ipv4)
    memset(&addr, 0, sizeof(struct sockaddr_in)); // memsetで初期化
    addr.sin_family = AF_INET; // アドレスファミリ(ipv4)

    // 送信関数
    auto send_func = [&]() {
        // メッセージのヘッダ情報を書き込む
        // バージョン: 4bit (0), 
        // メッセージID: 4bit (2),
        // メッセージに含まれるRWerの個数: 16bit
        memcpy(message, &ver_id, sizeof(ver_id));
        memcpy(message + sizeof(ver_id), &RWer_count, sizeof(RWer_count));
        now_length += sizeof(ver_id) + sizeof(RWer_count);

        // ポート番号指定
        addr.sin_port = htons(gen.genRandHostId(10000, 10000+RECV_PORT-1)); // ポート番号, htons()関数は16bitホストバイトオーダーをネットワークバイトオーダーに変換

        // データ送信
        sendto(sockfd, message, now_length, 0, (struct sockaddr *)&addr, sizeof(addr)); 

        // debug
        // std::cout << "send" << std::endl;

        // 変数初期化
        memset(message, 0, MESSAGE_MAX_LENGTH_SEND);
        RWer_count = 0;
        now_length = 0;
    };

    while (1) {
        // 送信先id取得
        host_id_t send_id = 0;
        {
            std::lock_guard<std::mutex> lk(mtx_id_num_);
            while (id_num_ == hostid_ || watching_queue_flag_[id_num_]) {
                id_num_ = (id_num_ + 1) % SEND_QUEUE_NUM;
            }
            send_id = id_num_;
            // debug
            // std::cout << "send_id: " << send_id << std::endl;
            watching_queue_flag_[send_id] = true;
            id_num_ = (id_num_ + 1) % SEND_QUEUE_NUM;
        }
        addr.sin_addr.s_addr = worker_ip_all_[send_id];

        // send_queue_ から RWer をまとめて取得
        if (send_queue_[send_id].getSize() == 0) {
            watching_queue_flag_[send_id] = false;
            continue;
        }
        std::vector<std::unique_ptr<RandomWalker>> RWer_ptr_vec;
        uint32_t vec_size = send_queue_[send_id].pop(RWer_ptr_vec);

        watching_queue_flag_[send_id] = false;

        // debug 
        // std::cout << "vec_size: " << vec_size << std::endl;

        int idx = 0;
        while (idx < vec_size) {
            // RWer データサイズ
            uint32_t RWer_data_length = RWer_ptr_vec[idx]->getRWerSize();

            if (now_length + RWer_data_length >= MESSAGE_MAX_LENGTH_SEND - sizeof(ver_id) - sizeof(RWer_count)) { // メッセージに収まりきらなくなったら送信
                send_func();
            }

            // RWerの中身をメッセージに詰める
            // memcpy(message + now_length, &RWer, RWer_data_length);
            RWer_ptr_vec[idx]->writeMessage(message + sizeof(ver_id) + sizeof(RWer_count) + now_length);
            now_length += RWer_data_length;
            RWer_count++;
            idx++;
        }

        // 残りを送信
        if (RWer_count > 0) send_func();
    }
}

inline void RandomWalkSystemWorker::receiveMessage(const uint16_t& port_num) {
    std::cout << "receiveMessage: " << port_num << std::endl;

    int sockfd = createUdpServerSocket(port_num);

    StdRandNumGenerator gen;

    while (1) {
        // messageを受信
        char message[MESSAGE_MAX_LENGTH_RECV];
        memset(message, 0, MESSAGE_MAX_LENGTH_RECV);
        recv(sockfd, message, MESSAGE_MAX_LENGTH_RECV, 0);

        uint8_t ver_id = *(uint8_t*)message;

        if ((ver_id & MASK_MESSEGEID) == START_EXP) { // 実験開始の合図

            uint32_t startmanager_ip = *(uint32_t*)(message + sizeof(ver_id));
            uint32_t num_RWer = *(uint32_t*)(message + sizeof(ver_id) + sizeof(startmanager_ip));

            startmanagerip_ = startmanager_ip;
            RW_config_.setNumberOfRWExecution(num_RWer);

            // debug
            std::cout << "num_RWer = " << num_RWer << std::endl;

            MAIN_EX = true;
            CHECK_RWER_FLAG = false;

            // 実験開始のフラグを立てる
            start_flag_.writeReady(true);

        } else if ((ver_id & MASK_MESSEGEID) == RWERS) { // RWer のメッセージ
            // message に入っている RWer の数を確認
            int idx = sizeof(uint8_t);
            uint16_t RWer_count = *(uint16_t*)(message + idx); idx += sizeof(uint16_t);

            std::vector<std::unique_ptr<RandomWalker>> RWer_ptr_vec(RWer_count);

            for (int i = 0; i < RWer_count; i++) {
                // std::unique_ptr<RandomWalker> RWer_ptr(new RandomWalker(message + idx));
                RWer_ptr_vec[i] = std::make_unique<RandomWalker>(message + idx);
                idx += RWer_ptr_vec[i]->getRWerSize();
            }

            // まとめて RWer キューに push
            if (MAIN_EX) RWer_queue_[gen.gen(PROC_MESSAGE_THREAD_NUM)].push(RWer_ptr_vec);
            else RWer_queue_[gen.gen(PROC_MESSAGE_CACHE_THREAD_NUM)].push(RWer_ptr_vec);

        } else if ((ver_id & MASK_MESSEGEID) == CACHE_GEN) { // キャッシュ生成用の RW 実行

            startmanagerip_ = *(uint32_t*)(message + sizeof(ver_id));
            MAIN_EX = false;
            CHECK_RWER_FLAG = true;
            CACHE_GEN_FLAG = true;
            start_cache_flag_.writeReady(true);

        } else if ((ver_id & MASK_MESSEGEID) == END_EXP) { // 実験結果を送信

            sendToStartManager();

        } else {
            perror("wrong id");
            exit(1); // 異常終了
        }
    }
}

inline int RandomWalkSystemWorker::createUdpServerSocket(const uint16_t& port_num) {
    // ソケットの生成
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    // debug
    // std::cout << sockfd << std::endl;
    if (sockfd < 0) { // エラー処理
        perror("socket");
        exit(1); // 異常終了
    }

    // アドレスの生成
    struct sockaddr_in addr; // 接続先の情報用の構造体(ipv4)
    memset(&addr, 0, sizeof(struct sockaddr_in)); // memsetで初期化
    addr.sin_family = AF_INET; // アドレスファミリ(ipv4)
    addr.sin_port = htons(port_num); // ポート番号, htons()関数は16bitホストバイトオーダーをネットワークバイトオーダーに変換
    addr.sin_addr.s_addr = hostip_; // IPアドレス, inet_addr()関数はアドレスの翻訳

    // ソケット登録
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { // ソケット, アドレスポインタ, アドレスサイズ // エラー処理
        perror("bind");
        exit(1); // 異常終了
    }

    return sockfd;
}

inline int RandomWalkSystemWorker::createTcpServerSocket(const uint16_t& port_num) {
    // ソケットの生成
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { // エラー処理
        perror("socket");
        exit(1); // 異常終了
    }

    // アドレスの生成
    struct sockaddr_in addr; // 接続先の情報用の構造体(ipv4)
    memset(&addr, 0, sizeof(struct sockaddr_in)); // memsetで初期化
    addr.sin_family = AF_INET; // アドレスファミリ(ipv4)
    addr.sin_port = htons(port_num); // ポート番号, htons()関数は16bitホストバイトオーダーをネットワークバイトオーダーに変換
    addr.sin_addr.s_addr = hostip_; // IPアドレス, inet_addr()関数はアドレスの翻訳

    // ソケット登録
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { // ソケット, アドレスポインタ, アドレスサイズ // エラー処理
        perror("bind");
        exit(1); // 異常終了
    } else {
        std::cout << "bind ok" << std::endl;
    }

    // 受信待ち
    if (listen(sockfd, SOMAXCONN) < 0) { // ソケット, キューの最大長 // エラー処理
        perror("listen");
        close(sockfd); // ソケットクローズ
        exit(1); // 異常終了
    } else {
        std::cout << "listen ok" << std::endl;    
    }

    return sockfd;
}

inline void RandomWalkSystemWorker::sendToStartManager() {
    // start manager に送信するのは, RW 終了数, 実行時間
    uint32_t end_count = RW_manager_.getEndcnt();
    double execution_time = RW_manager_.getExecutionTime();
    std::cout << "end_count : " << end_count << std::endl;
    std::cout << "execution_time : " << execution_time << std::endl;

    // debug
    std::cout << "RWer_queue_size: " << PROC_MESSAGE_THREAD_NUM << std::endl;
    for (int i = 0; i < PROC_MESSAGE_THREAD_NUM; i++) {
        std::cout << i << ": " << RWer_queue_[i].getSize() << std::endl;
    }
    std::cout << "send_queue_size: " << SEND_QUEUE_NUM << std::endl;
    for (int i = 0; i < SEND_QUEUE_NUM; i++) {
        std::cout << i << ": " << send_queue_[i].getSize() << std::endl;
    }
    std::cout << "re_send_count: " << re_send_count << std::endl;
    std::cout << "my edges num: " << graph_.getEdgeCount() << std::endl;
    std::cout << "cache edges num: " << cache_.getEdgeCount() << std::endl;
    std::cout << "all edges: " << graph_.getEdgeCount() + cache_.getEdgeCount() << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(5));
    {
        // ソケットの生成
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) { // エラー処理
            perror("socket");
            exit(1); // 異常終了
        }

        // アドレスの生成
        struct sockaddr_in addr; // 接続先の情報用の構造体(ipv4)
        memset(&addr, 0, sizeof(struct sockaddr_in)); // memsetで初期化
        addr.sin_family = AF_INET; // アドレスファミリ(ipv4)
        addr.sin_port = htons(9999); // ポート番号, htons()関数は16bitホストバイトオーダーをネットワークバイトオーダーに変換
        addr.sin_addr.s_addr = startmanagerip_; // IPアドレス, inet_addr()関数はアドレスの翻訳

        // ソケット接続要求
        connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)); // ソケット, アドレスポインタ, アドレスサイズ

        // データ送信 (hostip: 4B, end_count: 4B, all_execution_time: 8B)
        char message[MESSAGE_MAX_LENGTH_SEND];
        int idx = 0;
        memcpy(message + idx, &hostip_, sizeof(uint32_t)); idx += sizeof(uint32_t);
        memcpy(message + idx, &end_count, sizeof(uint32_t)); idx += sizeof(uint32_t);
        memcpy(message + idx, &execution_time, sizeof(double)); idx += sizeof(double);
        memcpy(message + idx, &re_send_count, sizeof(uint32_t)); idx += sizeof(uint32_t);
        send(sockfd, message, sizeof(message), 0); // 送信

        // ソケットクローズ
        close(sockfd); 
    }

    // // 再送用スレッドを停止させる
    // for (std::thread& th : re_send_threads_) {
    //     th.join();
    // }
    // re_send_threads_.clear();

}

