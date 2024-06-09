#pragma once

#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "type.hpp"

template<typename T>
void read_graph(const char* fname, T* &edge, edge_id_t &e_num)
{
    FILE *f = fopen(fname, "r");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    size_t total_size = ftell(f);
    size_t total_e_num = total_size / sizeof(T);
    e_num = total_e_num;
    edge = new T[e_num];
    fseek(f, 0, SEEK_SET);
    auto ret = fread(edge, sizeof(T), e_num, f);
    assert(ret == e_num);
    fclose(f);
}