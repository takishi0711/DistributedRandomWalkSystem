#pragma once

#include <iostream>

// 十分に大きな値
const uint32_t INF = 2001002003;

uint32_t MY_EDGE_NUM = 0;

// message_id_ の値
const uint32_t ALIVE = 0;
const uint32_t DEAD = 1;
const uint32_t RWERS = 2;
const uint32_t START_EXP = 3;
const uint32_t CACHE_GEN = 4;
const uint32_t END_EXP = 5;
const uint32_t DEAD_SEND = 6;
const uint32_t DUMMY = 7;

// ver_id_ のマスク
const uint32_t MASK_VER = (1<<7) + (1<<6) + (1<<5) + (1<<4);
const uint32_t MASK_MESSEGEID = (1<<3) + (1<<2) + (1<<1) + (1<<0);

// procMessage の中断用フラグ
bool PROC_MESSAGE_FLAG = true;

// RW 終了時に checkRWer をするかどうか
bool CHECK_RWER_FLAG = false;

// cache 用の実行を続けるためのフラグ
bool CACHE_GEN_FLAG = true;

// メインの実験が始まっているかどうか
bool MAIN_EX = true;



////////////////////////////////////////////////////
// 自分で設定

// RW の α
const double ALPHA = 0.15;

// データ構造の初期化用 (全グラフの頂点数の数よりも大きく設定する)
// 実装簡易化のために配列のサイズを先に指定する
// もしより現実的な実験する場合は全グラフの頂点数はわからないためこのパラメタは使うべきでない
// その場合 unordered_map 等を使ってグラフデータの管理を行うことになる
const uint64_t VERTEX_SIZE = 5000000;

// 「cacheエッジ数 + 元々持ってるエッジ数」の最大値
const uint32_t MAX_CACHE_SIZE = 200;

// cache 用の実行における RWer の最大生成数
const uint64_t MAX_RWER_NUM_FOR_CACHE = 100000; 

// RW の実行ステップ (RW_STEP 回実行するごとに少しスリープ) 
const uint32_t RW_STEP = 500000; // cache 補充用の実行

// RWer 生成の sleep 時間 (s)
const uint32_t GENERATE_SLEEP_TIME = 4; // cache 補充用の実行

// message 処理スレッドの数
uint32_t PROC_MESSAGE_THREAD_NUM = 15; // メイン実行用
uint32_t PROC_MESSAGE_CACHE_THREAD_NUM = 10; // cache 補充用の実行

// 送信キューの数 (サーバ数、グラフ分割数)
uint32_t SEND_QUEUE_NUM = 5;

// 受信スレッド数 (実験で使用するポート番号数)
const uint32_t RECV_PORT = 4;

// RWer 生成スレッド数
const uint32_t GENERATE_RWER_THREAD_NUM = 15; // メイン実行用
const uint32_t GENERATE_RWER_CACHE_THREAD_NUM = 4; // cache 補充用の実行

// メッセージ長
const uint32_t MESSAGE_MAX_LENGTH_SEND = 8950;
const uint32_t MESSAGE_MAX_LENGTH_RECV = 8950;