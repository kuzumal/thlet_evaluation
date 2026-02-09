#ifndef __CONST_H__
#define __CONST_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define NUM_SERVERS   10000
#define KEY_SIZE      10
#define VALUE_SIZE    100
#define SERVER_IP     "172.16.0.2"

#define PORT 6666

enum Type {
    Get, Put, Exit
};

enum State {
    Fail, Success
};

typedef struct {
    enum Type type;
    char key[KEY_SIZE];
    char value[VALUE_SIZE];
} Rpc;

typedef struct {
    enum State state;
    char value[VALUE_SIZE];
} Result;

#endif