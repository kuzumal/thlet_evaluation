#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>

void sender_listener(void *arg) {

  // sender code would go here
}

void receiver() {
    
}

int main() {

  driver_fd = open("/dev/uittmon", O_RDWR);
  // printf("driver_fd: %d\n", driver_fd);
#ifndef SENDDIFF
  int core1 = 20, core2 = 0;
#else

  int core1 = 20, core2 = 0;
#endif

  //  core2=0;
  // printf("Give me two sibling cores: <number> <number>\n");
  // scanf("%d %d ", core1,core2);
  // set_thread_affinity(core1);
  receiver_pid = getpid();
  char *num_str = getenv("NUM");
  num = 156;
  // bind sender to core 1
  pthread_attr_t attrs;
  memset(&attrs, 0, sizeof(attrs));
  pthread_attr_init(&attrs);
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(core2, &set);
  pthread_attr_setaffinity_np(&attrs, sizeof(set), &set);
  pthread_t pt;

  // get receiver pid
  receiver_pid = getpid();

  // bind receiver to core 56 (sibling)
  CPU_ZERO(&set);
  CPU_SET(core1, &set);
  pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

  if (pthread_create(&pt, &attrs, &sender_listener, NULL))
    exit(-1);
  reciever();
  pthread_join(pt, NULL);
  close(driver_fd);
}
