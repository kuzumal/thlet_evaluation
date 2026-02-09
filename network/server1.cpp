#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

#define SERVER_THREAD_NUM 10
#define WORKER_THREAD_NUM 100

enum RpcType {
    get, put
};

struct Rpc {
    RpcType type;
    char* data;
    uint32_t len;
};

int main () {

    return 0;
}