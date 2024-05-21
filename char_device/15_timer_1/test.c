/*
 * @Date: 2024-05-04 20:30:59
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-19 19:44:45
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

int main()
{
  int fd;
  int counter = 0;
  int old_counter = 0;

  // 打开/dev/timer设备文件
  fd = open("/dev/timer", O_RDONLY);
  if (fd != -1)
  {
    while (1)
    {
      read(fd, &counter, sizeof(unsigned int));
      if (counter != old_counter)
      {
        printf("/dev/timer :%d\n", counter);
        old_counter = counter;
      }
    }
  }
  else
  {
    printf("Device open failure\n");
  }
  return 0;
}
