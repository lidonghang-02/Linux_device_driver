/*
 * @Date: 2024-05-01 11:53:02
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-19 19:13:00
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main()
{
  char buf[10] = "hello";
  int len;
  int fd = open("/dev/sem_dev", O_RDWR);
  if (fd < 0)
  {
    printf("open failed");
    return 0;
  }
  int buf_len = strlen(buf);
  int i;
  for (i = 0; i < 10; i++)
  {
    len = write(fd, buf, buf_len);
    printf("...\n");
    sleep(1);
  }
  char res[100];
  len = read(fd, res, buf_len);
  buf[len] = '\0';
  printf("%d %s\n", buf_len, res);
  close(fd);
  return 0;
}
