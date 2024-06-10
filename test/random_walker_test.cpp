#include <iostream>

using namespace std;

#include "../include/random_walker.hpp"

int main() {
    RandomWalker RWer(1, 5, 0, 12345, 10);
    RWer.printRWer();

    RWer.updateRWer(2, 12345, 100, 111, 222);
    RWer.printRWer();

    cout << RWer.getPrevNodeID() << endl;

    RWer.updateRWer(3, 23456, 200, 333, 444);
    RWer.printRWer();

    cout << RWer.getPrevNodeID() << endl;

    char message[1000];
    RWer.writeMessage(message);

    RandomWalker RWer2(message);
    RWer2.printRWer();
    return 0;
}