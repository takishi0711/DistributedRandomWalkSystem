#pragma once

#include <vector>

#include "type.hpp"
#include "../config/param.hpp"
#include "cache_helper.hpp"
#include "random_walker.hpp"
#include "graph.hpp"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Cache {

public :

    void init();

    // 頂点に対するキャッシュの次数情報を入手
    index_t getDegree(const vertex_id_t& node_id);

    // 頂点の持ち主の ホストIDを入手
    host_id_t getHostId(const vertex_id_t& node_id);

    // キャッシュの次数存在確認
    bool hasDegree(const vertex_id_t& node_id);

    // 隣接リスト情報内の index 存在確認
    // 存在したら next node ID を返す
    // 存在しなかったら INF を返す
    vertex_id_t getNextNodeID(const vertex_id_t& node_id, const index_t& index_num);

    // RWer の経路情報からグラフデータをキャッシュとして保存
    void addRWer(std::unique_ptr<RandomWalker>&& RWer_ptr, Graph& graph);

    // エッジをキャッシュに登録
    void addEdge(const std::vector<vertex_id_t>& path, const index_t& node_u_idx, const index_t& node_v_idx, Graph& graph);

    // ホストID 情報を登録
    void registerHostId(const vertex_id_t& node_id, const host_id_t& host_id);

    // 次数情報を登録
    void registerDegree(const vertex_id_t& node_id, const index_t& degree);

    // インデックス情報を登録
    void registerIndex(const vertex_id_t& node_id_u, const vertex_id_t& node_id_v, const index_t& index_num);

    // キャッシュのエッジカウント 
    edge_id_t getEdgeCount();

private :

    // キャッシュ情報
    std::vector<index_t> degree_; // 他サーバが持ち主となるノードの次数
    std::vector<host_id_t> host_id_; // 持ち主が他サーバの頂点に関する, 持ち主のホストID {ノード ID : ホストID (ノードの持ち主)}
    SimpleCache adjacency_list_;
    std::vector<bool> has_v_;

};


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

inline void Cache::init() {
    degree_.resize(VERTEX_SIZE);
    host_id_.resize(VERTEX_SIZE);
    has_v_.resize(VERTEX_SIZE);
    adjacency_list_.init();
}

inline index_t Cache::getDegree(const vertex_id_t& node_id) {
    return degree_[node_id];
}

inline host_id_t Cache::getHostId(const vertex_id_t& node_id) {
    return host_id_[node_id];
}

inline bool Cache::hasDegree(const vertex_id_t& node_id) {
    return has_v_[node_id];
}

inline vertex_id_t Cache::getNextNodeID(const vertex_id_t& node_id, const index_t& index_num) {
    return adjacency_list_.getNextNodeID(node_id, index_num);
}

inline void Cache::addRWer(std::unique_ptr<RandomWalker>&& RWer_ptr, Graph& graph) {
    // debug
    // std::cout << "addRWer" << std::endl;

    uint16_t path_length = 0;
    std::vector<uint64_t> path; // path: (頂点, ホストID, 次数, indexuv, indexvu), (), (), ... 
    RWer_ptr->getPath(path_length, path);

    // debug
    // std::cout << "path_length: " << path_length << std::endl;
    // std::cout << "path size: " << path.size() << std::endl;

    for (int i = 0; i < path_length-1; i++) {
        // エッジをキャッシュに追加 (無向グラフ)
        addEdge(path, i*5, (i+1)*5, graph);

        // debug 
        // std::cout << i << std::endl;
    }

    // debug 
    // std::cout << "addRWer ok" << std::endl;
}

inline void Cache::addEdge(const std::vector<vertex_id_t>& path, const index_t& node_u_idx, const index_t& node_v_idx, Graph& graph) {
    // debug
    // std::cout << "AddEdge" << std::endl;

    uint32_t node_id_u = path[node_u_idx];
    uint32_t node_id_v = path[node_v_idx];
    uint32_t host_id_u = path[node_u_idx + 1];
    uint32_t host_id_v = path[node_v_idx + 1];
    uint32_t degree_u = path[node_u_idx + 2];
    uint32_t degree_v = path[node_v_idx + 2];
    uint32_t index_uv = path[node_v_idx + 3];
    uint32_t index_vu = path[node_v_idx + 4];

    if (!graph.hasVertex(node_id_u)) {
        registerHostId(node_id_u, host_id_u);
        if (degree_u != INF) registerDegree(node_id_u, degree_u);
        if (index_uv != INF) registerIndex(node_id_u, node_id_v, index_uv);
    }

    if (!graph.hasVertex(node_id_v)) {
        registerHostId(node_id_v, host_id_v);
        if (degree_v != INF) registerDegree(node_id_v, degree_v);
        if (index_vu != INF) registerIndex(node_id_v, node_id_u, index_vu);
    }
}

inline void Cache::registerHostId(const vertex_id_t& node_id, const host_id_t& host_id) {
    host_id_[node_id] = host_id;
}

inline void Cache::registerDegree(const vertex_id_t& node_id, const index_t& degree) {
    degree_[node_id] = degree;
    has_v_[node_id] = true;
}

inline void Cache::registerIndex(const vertex_id_t& node_id_u, const vertex_id_t& node_id_v, const index_t& index_num) {
    adjacency_list_.setIndex(node_id_u, index_num, node_id_v);
}

inline edge_id_t Cache::getEdgeCount() {
    return adjacency_list_.getSize();
}