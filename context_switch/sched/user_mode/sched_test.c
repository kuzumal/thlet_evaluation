#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdint.h>

#define DEV_PATH "/dev/thlet_sched"
int fd;

void* thread_func(void* arg) {
    char* name = (char*)arg;
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    for (int i = 0; i < 5; i++) {
        sched_yield(); 
        
        ioctl(fd, 0, 0); 
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;
    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    pthread_create(&t1, NULL, thread_func, "Thread-A");
    pthread_create(&t2, NULL, thread_func, "Thread-B");

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    close(fd);
    return 0;
}