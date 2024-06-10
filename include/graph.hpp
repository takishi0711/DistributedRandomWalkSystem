#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>

#include "type.hpp"
#include "storage.hpp"
#include "util.hpp"
#include "../config/param.hpp"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Graph {
    public :

    // グラフファイル読み込み
    void init(const std::string& dir_path, const std::string& host_id_str, const host_id_t& hostid);

    // 自サーバが持ち主となる頂点の数を入手
    vertex_id_t getMyVerticesNum();

    // 自サーバが持ち主となる頂点集合を入手
    std::vector<vertex_id_t> getMyVertices();

    // 頂点の持ち主の HostID を入手
    host_id_t getHostId(const vertex_id_t& node_id);

    // 頂点の次数を入手
    index_t getDegree(const vertex_id_t& node_id);

    // 頂点の持ち主が自サーバであるか確認
    bool hasVertex(const vertex_id_t& node_id);

    // 現在頂点とインデックスを引数にして次の頂点を返す
    vertex_id_t getNextNodeID(const vertex_id_t& current_node, const index_t& next_index, StdRandNumGenerator& gen);

    // 頂点 u, v を受け取り, u[x] = v の x を返す (index を返す)
    // 頂点 u が自分のサーバのものでない場合は INF を返す
    index_t indexOfUV(const vertex_id_t& node_id_u, const vertex_id_t& node_id_v);

    // グラフのエッジカウント 
    edge_id_t getEdgeCount();

    private:

    std::vector<vertex_id_t> my_vertices_vector_; // 自サーバが持ち主となる頂点集合 (配列)
    std::vector<host_id_t> vertices_host_id_; // 自サーバが保持している頂点の持ち主の IP アドレス {頂点 ID : IP アドレス (頂点の持ち主)}
    std::vector<std::vector<vertex_id_t>> adjacency_list_; // 自サーバの隣接リスト {頂点 ID : 隣接リスト}
    std::vector<index_t> degree_; // 自サーバが持ち主となる頂点の次数
    std::vector<bool> has_v_;
    edge_id_t edge_count_;

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


inline void Graph::init(const std::string& dir_path, const std::string& host_id_str, const host_id_t& hostid) {
    std::string graph_file_path = dir_path + host_id_str + ".data"; // グラフファイルのパス
    Edge_dstIp *read_edges;
    edge_id_t read_e_num;
    read_graph(graph_file_path.c_str(), read_edges, read_e_num);
    edge_count_ = read_e_num;
    MY_EDGE_NUM = edge_count_;
    std::cout << "MY_EDGE_NUM: " << MY_EDGE_NUM << std::endl;

    // node_id の最大値を確認
    vertex_id_t mx_id = 0;
    for (int e_i = 0; e_i < read_e_num; e_i++) {
        mx_id = std::max((vertex_id_t)read_edges[e_i].src, mx_id);
    }

    // データ構造のサイズ指定
    vertices_host_id_.resize(VERTEX_SIZE);
    adjacency_list_.resize(mx_id+1);
    degree_.resize(mx_id+1);
    has_v_.resize(VERTEX_SIZE);

    // エッジデータを入れていく
    std::unordered_set<vertex_id_t> v_st;
    for (int e_i = 0; e_i < read_e_num; e_i++) {
        auto e = read_edges[e_i];
        v_st.insert(e.src);
        vertices_host_id_[e.src] = hostid;
        vertices_host_id_[e.dst] = e.dst_ip;
        adjacency_list_[e.src].push_back(e.dst);
        has_v_[e.src] = true;
    }
    for (vertex_id_t v : v_st) {
        degree_[v] = adjacency_list_[v].size();
        sort(adjacency_list_[v].begin(), adjacency_list_[v].end());
        my_vertices_vector_.push_back(v);
    }

}

inline vertex_id_t Graph::getMyVerticesNum() {
    return my_vertices_vector_.size();
}

inline std::vector<vertex_id_t> Graph::getMyVertices() {
    return my_vertices_vector_;
}

inline host_id_t Graph::getHostId(const vertex_id_t& node_id) {
    try {
        return vertices_host_id_[node_id];
    } catch (std::out_of_range& oor) {
        std::cerr << "graph getHostId node_id: " << node_id << std::endl;
        exit(1);
    }
}

inline index_t Graph::getDegree(const vertex_id_t& node_id) {
    try {
        return degree_[node_id];
    } catch (std::out_of_range& oor) {
        std::cerr << "graph getDegree node_id: " << node_id << std::endl;
        exit(1);
    }
}

inline bool Graph::hasVertex(const vertex_id_t& node_id) {
    assert(node_id < VERTEX_SIZE);
    return has_v_[node_id];
}

inline vertex_id_t Graph::getNextNodeID(const vertex_id_t& current_node, const vertex_id_t& next_index, StdRandNumGenerator& gen) {
    if (degree_[current_node] <= next_index) { // はみ出てる時
        std::cout << "segfault at getNextNode" << std::endl;
        return adjacency_list_[current_node][gen.gen(degree_[current_node])];
    }

    try {
        return adjacency_list_[current_node][next_index];
    } catch (std::out_of_range& oor) {
        std::cerr << "graph getNextNode current_node: " << current_node << std::endl;
        exit(1);
    }
}

inline index_t Graph::indexOfUV(const vertex_id_t& node_id_u, const vertex_id_t& node_id_v) {
    if (!hasVertex(node_id_u)) {
        // debug
        std::cout << "don't have " << node_id_u << std::endl;
        return INF;
    }
    index_t idx = std::lower_bound(adjacency_list_[node_id_u].begin(), adjacency_list_[node_id_u].end(), node_id_v) - adjacency_list_[node_id_u].begin();
    return idx;
}

inline edge_id_t Graph::getEdgeCount() {
    return edge_count_;
}