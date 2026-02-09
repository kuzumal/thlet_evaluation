#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include "const.h"

#define MAX_REQ_PER_THREAD 1000

typedef struct {
    size_t thread_id;
    size_t requst_numbers;
} Arg;

pthread_mutex_t lock;
int numbers;
long long lat[NUM_SERVERS * MAX_REQ_PER_THREAD];

bool cmp(const long long *a, const long long *b) { return (*a) - (*b) >= 0; }

long long timespec_diff_ns(const struct timespec *start, const struct timespec *end) {
    long long sec = (long long)(end->tv_sec - start->tv_sec);
    long long nsec = (long long)(end->tv_nsec - start->tv_nsec);
    return sec * 1000000000LL + nsec;
}

void generate_random_str(char* value, size_t len) {
    for (int i = 0; i < len; i ++) {
        int random_value = rand() % 62, c = 0;
        if (random_value < 26)
            c = 'A' + random_value; // Uppercase letters
        else if (random_value < 52)
            c = 'a' + (random_value - 26); // Lowercase letters
        else
            c = '0' + (random_value - 52); // Digits
        value[i] = c;
        if (i == len - 1) {
            value[i] = '\0';
        }
    }
}

void* generate_reqs(void* args) {
    Arg arg = *(Arg *)args;
    size_t req_nums = arg.requst_numbers;

    Rpc rpc[req_nums + 1];
    long long latency[req_nums + 1];

    for(int i = 0; i <= req_nums; i ++) {
        rpc[i].type = i == req_nums ? Exit : rand() % 2;
        generate_random_str(rpc[i].key, KEY_SIZE);
        generate_random_str(rpc[i].value, VALUE_SIZE);
    }

    // connect to server
    int sock = 0;
    struct sockaddr_in serv_addr;
    printf("Connecting ...\n");

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error in thread");
        return NULL;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported in thread");
        close(sock);
        return NULL;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Thread Connection Failed.\n");
        close(sock);
        return NULL;
    }
    
    printf("Connected\n"); fflush(stdout);

    double sum_latency = 0;
    struct timespec t0, t1;
    // waiting server to spawn and call recv
    usleep(10);

    // send requests
    for (int i = 0; i <= req_nums; i ++) {
        printf("Sending...\n"); fflush(stdout);
        send(sock, &rpc[i], sizeof(rpc[i]), 0);

        // printf("Receiving...\n"); fflush(stdout);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        recv(sock, &rpc[i], sizeof(int), 0);
        clock_gettime(CLOCK_MONOTONIC, &t1);

        printf("Received!\n"); fflush(stdout);
        latency[i] = timespec_diff_ns(&t0, &t1);
        sum_latency += latency[i];
    }
    close(sock);

    printf("Thread-%d avg_latency: %lf\n", arg.thread_id, sum_latency / (req_nums + 1));
    fflush(stdout);

    pthread_mutex_lock(&lock);
    for (int i = 0; i <= req_nums; i ++) {
        lat[numbers ++] = latency[i];
    }
    pthread_mutex_unlock(&lock);

    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: client [num-of-thread] [num-of-requests]\n");
        return 1;
    }
    int thread_numbers = atoi(argv[1]);
    size_t request_numbers = atoi(argv[2]);

    pthread_mutex_init(&lock, NULL);
    pthread_t threads[NUM_SERVERS];
    Arg args[NUM_SERVERS];

    printf("Spwan %d threads, each %d requests\n", thread_numbers, request_numbers);

    for (int i = 0; i < thread_numbers; i++) {
        args[i].thread_id = i;
        args[i].requst_numbers = request_numbers;
        if (pthread_create(&threads[i], NULL, generate_reqs, (void *)&(args[i])) != 0) {
            perror("Thread creation failed");
            return 1;
        }
    }

    for (int i = 0; i < thread_numbers; i++) {
        pthread_join(threads[i], NULL);
    }

    qsort(lat, numbers, sizeof(long long), cmp);
    double sum = 0;
    for (int i = 0; i < numbers; i ++) {
        sum += lat[i];
        printf("lat: %lld\n", lat[i]);
    }
    if (numbers != 0)
        printf("99-latency: %lldns avg-latency: %lfns\n", lat[numbers * 99 / 100], sum / numbers);
    fflush(stdout);

    return 0;
}