#include <stdio.h>
#include <time.h>
int main() {
   struct timespec ts;
   // 获取系统实时时间
   if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
       printf("当前时间: %ld 秒和 %ld 纳秒\n", ts.tv_sec, ts.tv_nsec);
   } else {
       perror("clock_gettime 调用失败");
   }
   return 0;
}