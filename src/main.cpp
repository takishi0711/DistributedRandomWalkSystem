#include <string>

#include "../include/random_walk_system_worker.hpp"


int main(int argc, char *argv[]) {
    // char* から string に変換
    std::string dir_path = argv[1]; 

    // workerを起動 
    RandomWalkSystemWorker rwsw(dir_path); 
    
    return 0;
}