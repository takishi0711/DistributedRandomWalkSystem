#pragma once

#include <string>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <unordered_map>

#include "../config/param.hpp"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class StartManager {

public :

    // コンストラクタ
    StartManager(const uint32_t& split_num);

    // cache 補充のための RW 実行合図
    void sendStartCache();
    
    // 実験開始の合図
    void sendStart(std::ofstream& ofs_time, std::ofstream& ofs_rerun, const int32_t RW_num);

    // 実験終了の合図
    void sendEnd(std::ofstream& ofs_time, std::ofstream& ofs_rerun);

    // IPv4 サーバソケットを作成 (UDP)
    int createUdpServerSocket();

    // IPv4 サーバソケットを作成 (TCP)
    int createTcpServerSocket();

private :

    std::string hostname_; // StartManager のホスト名
    uint32_t hostip_; // StartManager の IP アドレス
    std::string hostip_str_; // IP アドレスの文字列
    uint32_t RW_execution_num_ = 0;
    std::vector<uint32_t> worker_ip_; // 実験で使う通信先 IP アドレス
    uint32_t split_num_ = 0;

    const size_t MESSAGE_LENGTH = 250;

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

inline StartManager::StartManager(const uint32_t& split_num) {
    // 自分のホスト名
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
    ifr.ifr_addr.sa_family = AF_INET; // IPv4のIPアドレスを取得したい
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
            worker_ip_.emplace_back(inet_addr(reading_line_buffer.c_str()));
        }
    }

    split_num_ = split_num;
}

inline void StartManager::sendStartCache() {
    {
        // ソケットの生成
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) { // エラー処理
            perror("socket");
            exit(1); // 異常終了
        }   

        std::cout << "start cache" << std::endl;

        for (int i = 0; i < split_num_; i++) {
            // アドレスの生成
            struct sockaddr_in addr; // 接続先の情報用の構造体(ipv4)
            memset(&addr, 0, sizeof(struct sockaddr_in)); // memsetで初期化
            addr.sin_family = AF_INET; // アドレスファミリ(ipv4)
            addr.sin_port = htons(10000); // ポート番号, htons()関数は16bitホストバイトオーダーをネットワークバイトオーダーに変換
            addr.sin_addr.s_addr = worker_ip_[i]; // IPアドレス, inet_addr()関数はアドレスの翻訳        

            // メッセージ生成 (id: 1B, IPアドレス: 4B)
            char message[MESSAGE_LENGTH];

            // メッセージのヘッダ情報を書き込む
            // バージョン: 4bit (0), 
            // メッセージID: 4bit ,
            uint8_t ver_id = CACHE_GEN; 
            memcpy(message, &ver_id, sizeof(uint8_t));
            memcpy(message + sizeof(ver_id), &hostip_, sizeof(hostip_));

            // データ送信
            sendto(sockfd, message, MESSAGE_LENGTH, 0, (struct sockaddr *)&addr, sizeof(addr)); // 送信

            // debug
            // std::this_thread::sleep_for(std::chrono::seconds(5));
        } 

        close(sockfd);
    }

    // split_num 個のサーバから終了報告を受け取る
    {
        int count = 0; // 終了の合図が来た回数
        int sockfd = createTcpServerSocket(); // サーバソケットを生成 (TCP)

        uint32_t RWer_id_all_sum = 0;

        while (count < split_num_) {
            // 接続待ち
            std::cout << "wait" << std::endl;
            struct sockaddr_in get_addr; // 接続相手のソケットアドレス
            socklen_t len = sizeof(struct sockaddr_in); // 接続相手のアドレスサイズ
            int connect = accept(sockfd, (struct sockaddr *)&get_addr, &len); // 接続待ちソケット, 接続相手のソケットアドレスポインタ, 接続相手のアドレスサイズ
            if (connect < 0) { // エラー処理
                perror("accept");
                exit(1); // 異常終了
            }       
            std::cout << "connect" << std::endl;

            char message[1024]; // 受信バッファ
            memset(message, 0, sizeof(message)); // 受信バッファ初期化
            recv(connect, message, sizeof(message), 0); // 受信 (hostip: 4B, execution_time: 8B)
            
            uint32_t* worker_ip = (uint32_t*)message;
            // double* execution_time = (double*)(message + sizeof(uint32_t));
            uint32_t* RWer_id_all = (uint32_t*)(message + sizeof(uint32_t));
            // std::cout << "worker_ip: " << *worker_ip << ", execution_time: " << *execution_time << std::endl;
            std::cout << "worker_ip: " << *worker_ip << ", RWer_id_all: " << *RWer_id_all << std::endl;
            RWer_id_all_sum += *RWer_id_all;

            close(connect); // acceptしたソケットをclose

            count++;
        }
        close(sockfd);

        std::cout << "ave_RWer_id: " << RWer_id_all_sum / split_num_ << std::endl;
    }

    // 全てのサーバで終了したことを伝える
    {
        // ソケットの生成
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) { // エラー処理
            perror("socket");
            exit(1); // 異常終了
        }

        for (int i = 0; i < split_num_; i++) {
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
            addr.sin_addr.s_addr = worker_ip_[i]; // IPアドレス, inet_addr()関数はアドレスの翻訳

            // ソケット接続要求
            connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)); // ソケット, アドレスポインタ, アドレスサイズ

            // データ送信 
            char message[MESSAGE_LENGTH];
            int idx = 0;

            send(sockfd, message, sizeof(message), 0); // 送信
            
        }

        close(sockfd); 
    }

}

inline void StartManager::sendStart(std::ofstream& ofs_time, std::ofstream& ofs_rerun, const int32_t RW_num) {
    // ソケットの生成
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { // エラー処理
        perror("socket");
        exit(1); // 異常終了
    }    

    RW_execution_num_ = RW_num;

    std::cout << "start" << std::endl;

    for (int i = 0; i < split_num_; i++) {
        // アドレスの生成
        struct sockaddr_in addr; // 接続先の情報用の構造体(ipv4)
        memset(&addr, 0, sizeof(struct sockaddr_in)); // memsetで初期化
        addr.sin_family = AF_INET; // アドレスファミリ(ipv4)
        addr.sin_port = htons(10000); // ポート番号, htons()関数は16bitホストバイトオーダーをネットワークバイトオーダーに変換
        addr.sin_addr.s_addr = worker_ip_[i]; // IPアドレス, inet_addr()関数はアドレスの翻訳        

        // メッセージ生成 (id: 1B, IPアドレス: 4B, RW 実行回数: 4B)
        char message[MESSAGE_LENGTH];

        // メッセージのヘッダ情報を書き込む
        // バージョン: 4bit (0), 
        // メッセージID: 4bit (3),
        uint8_t ver_id = START_EXP; 
        memcpy(message, &ver_id, sizeof(uint8_t));
        memcpy(message + sizeof(ver_id), &hostip_, sizeof(hostip_));
        memcpy(message + sizeof(ver_id) + sizeof(hostip_), &RW_execution_num_, sizeof(RW_execution_num_));

        // データ送信
        sendto(sockfd, message, MESSAGE_LENGTH, 0, (struct sockaddr *)&addr, sizeof(addr)); // 送信

        // debug
        // std::this_thread::sleep_for(std::chrono::seconds(5));
    } 

    // ソケットクローズ
    close(sockfd);  
}

inline void StartManager::sendEnd(std::ofstream& ofs_time, std::ofstream& ofs_rerun) {
    // split_num 個のサーバに合図を送信
    for (int i = 0; i < split_num_; i++) {
        // ソケットの生成
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) { // エラー処理
            perror("socket");
            exit(1); // 異常終了
        }    

        // アドレスの生成
        struct sockaddr_in addr; // 接続先の情報用の構造体(ipv4)
        memset(&addr, 0, sizeof(struct sockaddr_in)); // memsetで初期化
        addr.sin_family = AF_INET; // アドレスファミリ(ipv4)
        addr.sin_port = htons(10000); // ポート番号, htons()関数は16bitホストバイトオーダーをネットワークバイトオーダーに変換
        addr.sin_addr.s_addr = worker_ip_[i]; // IPアドレス, inet_addr()関数はアドレスの翻訳

        // メッセージ生成 (id: 1B)
        char message[MESSAGE_LENGTH];

        // メッセージのヘッダ情報を書き込む
        // バージョン: 4bit (0), 
        // メッセージID: 4bit (4),
        uint8_t ver_id = END_EXP; 
        memcpy(message, &ver_id, sizeof(uint8_t));

        // データ送信
        sendto(sockfd, message, MESSAGE_LENGTH, 0, (struct sockaddr *)&addr, sizeof(addr)); // 送信 

        // ソケットクローズ
        close(sockfd);      

        // debug
        std::cout << i << std::endl;     
    }

    // split_num 個のサーバから実験結果を受け取る
    int count = 0; // 終了の合図が来た回数
    int sum_end_count = 0; // end_count の総和
    double max_all_execution_time = 0; // 最後の RWer が終了するときまでの時間
    int sockfd = createTcpServerSocket(); // サーバソケットを生成 (TCP)

    while (count < split_num_) {
        // 接続待ち
        struct sockaddr_in get_addr; // 接続相手のソケットアドレス
        socklen_t len = sizeof(struct sockaddr_in); // 接続相手のアドレスサイズ
        int connect = accept(sockfd, (struct sockaddr *)&get_addr, &len); // 接続待ちソケット, 接続相手のソケットアドレスポインタ, 接続相手のアドレスサイズ
        if (connect < 0) { // エラー処理
            perror("accept");
            exit(1); // 異常終了
        }       

        char message[1024]; // 受信バッファ
        memset(message, 0, sizeof(message)); // 受信バッファ初期化
        recv(connect, message, sizeof(message), 0); // 受信 (hostip: 4B, end_count: 4B, all_execution_time: 8B)
        
        uint32_t* worker_ip = (uint32_t*)message;
        uint32_t* end_count = (uint32_t*)(message + sizeof(uint32_t));
        double* execution_time = (double*)(message + sizeof(uint32_t) + sizeof(uint32_t));

        sum_end_count += *end_count;
        
        max_all_execution_time = std::max(max_all_execution_time, *execution_time);

        close(connect); // acceptしたソケットをclose

        count++;
    }

    // int drop_UDP = RW_execution_num_*split_num_*subgraph_size_ - sum_end_count;
    // std::cout << "drop_UDP : " << drop_UDP << std::endl;
    std::cout << "sum_end_count : " << sum_end_count << std::endl;
    std::cout << "max_all_execution_time : " << max_all_execution_time << std::endl;

    ofs_time << max_all_execution_time << std::endl; 
    // ofs_rerun << (double)drop_UDP / (split_num_*RW_execution_num_*subgraph_size_) * 100 << std::endl; 

    // サーバソケットクローズ
    close(sockfd); 
}

inline int StartManager::createUdpServerSocket() {
    // ソケットの生成
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { // エラー処理
        perror("socket");
        exit(1); // 異常終了
    }

    // アドレスの生成
    struct sockaddr_in addr; // 接続先の情報用の構造体(ipv4)
    memset(&addr, 0, sizeof(struct sockaddr_in)); // memsetで初期化
    addr.sin_family = AF_INET; // アドレスファミリ(ipv4)
    addr.sin_port = htons(9999); // ポート番号, htons()関数は16bitホストバイトオーダーをネットワークバイトオーダーに変換
    addr.sin_addr.s_addr = hostip_; // IPアドレス, inet_addr()関数はアドレスの翻訳

    // ソケット登録
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { // ソケット, アドレスポインタ, アドレスサイズ // エラー処理
        perror("bind");
        exit(1); // 異常終了
    }

    return sockfd;
}

inline int StartManager::createTcpServerSocket() {
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
    addr.sin_addr.s_addr = hostip_; // IPアドレス, inet_addr()関数はアドレスの翻訳

    int yes = 1;

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes)) < 0) {
        perror("ERROR on setsockopt");
        exit(1);
    }

    // ソケット登録
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { // ソケット, アドレスポインタ, アドレスサイズ // エラー処理
        perror("bind");
        exit(1); // 異常終了
    }

    // 受信待ち
    if (listen(sockfd, SOMAXCONN) < 0) { // ソケット, キューの最大長 // エラー処理
        perror("listen");
        close(sockfd); // ソケットクローズ
        exit(1); // 異常終了
    }

    return sockfd;
}