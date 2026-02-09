#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "const.h"
#include "hash.h"

Hash table;

bool handle_rpc(Rpc *rpc, int client) {
    char value[VALUE_SIZE];

    if (rpc->type == Get) {
        get(rpc->key, value, &table);
        return false;
    } else if (rpc->type == Put) {
        put(rpc->key, rpc->value, &table);
        return false;
    } else {
        return true;
    }
}

void* server(void *args) {
    int client = *(int *)args;
    char buffer[sizeof(Rpc)];
    char value[VALUE_SIZE];

    int size;
    while ((size = recv(client, buffer, sizeof(buffer), NULL)) > 0) {
        Rpc *rpc = (Rpc *)buffer;
        // do handle rpc
        if (handle_rpc(rpc, client)) {
            send(client, rpc, sizeof(int), 0);
            break;
        }
        send(client, rpc, sizeof(int), 0);
    }

    close(client);
    return NULL;
}

void init() {
    hash_init(&table);
}

void destroy() {
    hash_free(&table);
}

int main () {
    pthread_t thread[NUM_SERVERS];
    int client[NUM_SERVERS];
    int threads = 0;

    init();

    // Set the socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 64);

    // Loop and connect to clients
    while (threads < NUM_SERVERS) {
        struct sockaddr_in addr;
        socklen_t client_len = sizeof(addr);
        
        client[threads] = accept(listen_fd, (struct sockaddr*)&addr, &client_len);
        if (client[threads] < 0) {
            perror("Accept failed");
            destroy();
            return -1;
        }
        printf("Accept client\n");
        pthread_create(&thread[threads], NULL, server, (void *)&(client[threads]));
        threads ++;
    }

    for (int i = 0; i < threads; i ++) {
        pthread_join(thread[i], NULL);
    }

    destroy();

    return 0;
}