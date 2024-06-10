#include <assert.h>
#include <iostream>
#include <string>
#include <vector>

#include "../include/type.hpp"
#include "../include/storage.hpp"

using namespace std;

int main() {
    std::string str;
    std::cout << "filename" << std::endl;
    std::cin >> str;
    int split_num = 0;
    std::cout << "split_num" << std::endl;
    std::cin >> split_num;
    std::string ans;
    std::cout << "元々無向グラフかどうか(Yes or No)" << std::endl;
    cin >> ans;

    string input_path = "./source_graph/" + str + ".txt";

    // サーバー情報読み取り
    vector<string> server_id;
    FILE *f = fopen("../config/server.txt", "r");
    assert(f != NULL);
    char ch[100];
    while (1 == fscanf(f, "%s", &ch))
    {
        string str(ch);
        server_id.push_back(str);
    }
    fclose(f);
    // for (auto id : server_id) {
    //     cout << id << endl;
    // }

    // converted graph 読み取り
    vector<vector<Edge_dstIp>> edges(split_num);
    FILE *in_f = fopen(input_path.c_str(), "r");
    assert(in_f != NULL);
    vertex_id_t src, dst;
    while (2 == fscanf(in_f, "%lu %lu", &src, &dst))
    {
        if (ans == "Yes") {
            // cout << src%split_num << " " << src << " " << dst << " " << dst%split_num << endl;
            edges[src%split_num].push_back(Edge_dstIp(src, dst, dst%split_num));
        } else {
            edges[src%split_num].push_back(Edge_dstIp(src, dst, dst%split_num));
            edges[dst%split_num].push_back(Edge_dstIp(dst, src, src%split_num));
        }
    }
    fclose(in_f);

    for (int i = 0; i < split_num; i++) {
        auto es = edges[i].data();
        auto e_num = edges[i].size();
        string output_path = "./split_graph/" + str + "/" + to_string(split_num) + "/" + server_id[i] + ".data";
        FILE *out_f = fopen(output_path.c_str(), "w");
        assert(out_f != NULL);
        auto ret = fwrite(es, sizeof(Edge_dstIp), e_num, out_f);
        assert(ret == e_num);
        fclose(out_f);
    }

    // // test
    // Edge_dstIp *read_edges;
    // edge_id_t read_e_num;
    // read_graph("./split_graph/karate/5/10.58.60.3.data", read_edges, read_e_num);

    // FILE *test_f = fopen("test.txt", "w");
    // for (int i = 0; i < read_e_num; i++) {
    //     fprintf(test_f, "%d %d %d\n", read_edges[i].src, read_edges[i].dst, read_edges[i].dst_ip);
    // }
    // fclose(test_f);
    return 0;
}