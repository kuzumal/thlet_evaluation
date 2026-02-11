#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <endian.h>
#include "const.h"

#define SERVER "172.16.0.2"
#define RPC_MAGIC 0x7777

int main() {
  int sockfd;
  struct sockaddr_in serverAddr;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("socket failed");
    return 1;
  }

  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(PORT);
  
  if (inet_pton(AF_INET, SERVER, &serverAddr.sin_addr) <= 0) {
    perror("inet_pton failed");
    close(sockfd);
    return 1;
  }

  Rpc rpc;
  memset(&rpc, 0, sizeof(Rpc));
  
  rpc.magic = htobe64(RPC_MAGIC); 
  rpc.type = (uint32_t)Put;
  
  strncpy(rpc.key, "user", KEY_SIZE - 1);
  strncpy(rpc.value, "xxxxxxxxxxxx", VALUE_SIZE - 1);

  printf("Sending RPC requests...\n");

  ssize_t iResult;
  for (int i = 0; i < 105000; i++) {
    iResult = sendto(sockfd, &rpc, sizeof(Rpc), 0,
                      (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        
    if (iResult < 0) {
      perror("sendto failed");
      close(sockfd);
      return 1;
    }
  }

  printf("Last send: %zd bytes. Magic sent: 0x%lx\n", iResult, (unsigned long)RPC_MAGIC);

  close(sockfd);

  return 0;
}