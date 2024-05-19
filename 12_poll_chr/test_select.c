/*
 * @Date: 2024-05-12 17:31:54
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-19 19:18:40
 */
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "poll_chr.h"

int main()
{
  fd_set rfds, wfds; /* 读/写文件描述符集 */

  int fd = open("/dev/poll_chr0", O_RDONLY | O_NONBLOCK);
  if (fd != -1)
  {
    if (ioctl(fd, POLL_CHR_CLEAR, 0) < 0)
      printf("ioctl error\n");
    else
    {
      while (1)
      {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(fd, &rfds);
        FD_SET(fd, &wfds);
        select(fd + 1, &rfds, &wfds, NULL, NULL);
        /* 数据可获得 */
        if (FD_ISSET(fd, &rfds))
          printf("Poll monitor:can be read\n");
        /* 数据可写入 */
        if (FD_ISSET(fd, &wfds))
          printf("Poll monitor:can be written\n");
        sleep(1);
      }
    }
  }
  else
    printf("open error\n");
  return 0;
}
