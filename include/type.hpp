#pragma once

#include <stdint.h>
#include <utility>

typedef uint64_t vertex_id_t;
typedef uint64_t edge_id_t;
typedef uint64_t walker_id_t;
typedef uint32_t host_id_t;
typedef uint16_t worker_id_t;
typedef uint64_t index_t;

struct EmptyData
{
};

template <typename edge_data_t>
struct Edge
{
    vertex_id_t src;
    vertex_id_t dst;
    edge_data_t data;
    Edge() {}
    Edge(vertex_id_t _src, vertex_id_t _dst, edge_data_t _data) : src(_src), dst(_dst), data(_data) {}
    bool friend operator == (const Edge<edge_data_t> &a, const Edge<edge_data_t> &b)
    {
        return (a.src == b.src
            && a.dst == b.dst
            && a.data == b.data
        );
    }
    void transpose()
    {
        std::swap(src, dst);
    }
};

template <>
struct Edge <EmptyData>
{
    vertex_id_t src;
    union
    {
        vertex_id_t dst;
        EmptyData data;
    };
    Edge() {}
    Edge(vertex_id_t _src, vertex_id_t _dst) : src(_src), dst(_dst) {}
    bool friend operator == (const Edge<EmptyData> &a, const Edge<EmptyData> &b)
    {
        return (a.src == b.src
            && a.dst == b.dst
        );
    }
    void transpose()
    {
        std::swap(src, dst);
    }
};

struct Edge_dstIp
{
    vertex_id_t src;
    vertex_id_t dst;
    uint8_t dst_ip;

    Edge_dstIp() {}
    Edge_dstIp(vertex_id_t _src, vertex_id_t _dst, uint8_t _dst_ip) : src(_src), dst(_dst), dst_ip(_dst_ip) {}
    bool friend operator == (const Edge_dstIp &a, const Edge_dstIp &b)
    {
        return (a.src == b.src
            && a.dst == b.dst
        );
    }
    void transpose()
    {
        std::swap(src, dst);
    }
};

template<typename edge_data_t>
struct AdjUnit
{
    vertex_id_t neighbour;
    edge_data_t data;
};

template<>
struct AdjUnit<EmptyData>
{
    union
    {
        vertex_id_t neighbour;
        EmptyData data;
    };
};

template<typename edge_data_t>
struct AdjList
{
    AdjUnit<edge_data_t> *begin;
    AdjUnit<edge_data_t> *end;
    void init()
    {
        begin = nullptr;
        end = nullptr;
    }
};

//comprised column row
template<typename edge_data_t>
struct EdgeContainer
{
    AdjList<edge_data_t> *adj_lists;
    AdjUnit<edge_data_t> *adj_units;
    EdgeContainer() : adj_lists(nullptr), adj_units(nullptr) {}
    ~EdgeContainer()
    {
        if (adj_lists != nullptr)
        {
            delete []adj_lists;
        }
        if (adj_units != nullptr)
        {
            delete []adj_units;
        }
    }
};