/*
 * @Date: 2024-06-03 11:48:02
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-06-03 11:59:46
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

static void signalio_handler(int signum)
{
    printf("receive a signal from kfifo_dev,signalnum:%d\n", signum);
}

void main(void)
{
    int fd, oflags;
    fd = open("/dev/kfifo_dev", O_RDWR, S_IRUSR | S_IWUSR);
    if (fd != -1)
    {
        signal(SIGIO, signalio_handler);
        fcntl(fd, F_SETOWN, getpid());
        oflags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, oflags | FASYNC);
        while (1)
        {
            sleep(100);
        }
    }
    else
    {
        printf("device open failure\n");
    }
}
