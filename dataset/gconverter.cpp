#include <assert.h>
#include <iostream>
#include <string>
#include <vector>

#include "../include/type.hpp"

using namespace std;

int main() {
    std::string str;
    std::cout << "filename" << std::endl;
    std::cin >> str;
    string input_path = "./source_graph/" + str + ".txt";
    string output_path = "./converted_graph/" + str + ".data";

    std::vector<Edge<EmptyData> > edges;

    FILE *f = fopen(input_path.c_str(), "r");
    assert(f != NULL);
    vertex_id_t src, dst;
    while (2 == fscanf(f, "%lu %lu", &src, &dst))
    {
        edges.push_back(Edge<EmptyData>(src, dst));
    }
    fclose(f);

    auto es = edges.data();
    auto e_num = edges.size();
    FILE *out_f = fopen(output_path.c_str(), "w");
    assert(out_f != NULL);
    auto ret = fwrite(es, sizeof(Edge<EmptyData>), e_num, out_f);
    assert(ret == e_num);
    fclose(out_f);
    
}