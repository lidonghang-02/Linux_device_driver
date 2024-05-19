/*
 * @Date: 2024-05-07 16:54:27
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-19 19:54:51
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "ioctl.h"
int main(int argc, char* argv[])
{
  unsigned int n;
  int fd;
  fd = open("/dev/ioctl0", O_RDWR);
  switch (argv[1][0])
  {
  case '0':
    ioctl(fd, IOCTL_CLEAN);
    printf("rclean mem\n");
    break;
  case '1':
    ioctl(fd, IOCTL_GET_LEN, &n);
    printf("get lens value = %u\n", n);
    break;
  case '2':
    n = atoi(argv[2]);
    ioctl(fd, IOCTL_SET_LEN, &n);
    printf("set lens value = %u\n", n);
    break;
  }
  close(fd);

  return 0;
}
