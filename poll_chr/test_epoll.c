#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <strings.h>

#include <sys/epoll.h>

#include "poll_chr.h"

int main()
{

    int fd = open("/dev/poll_chr0", O_RDONLY | O_NONBLOCK);

    if (fd != -1)
    {
        struct epoll_event ev_poll_chr;
        int err;
        int epfd;
        if (ioctl(fd, POLL_CHR_CLEAR, 0) < 0)
        {
            printf("ioctl error\n");
            return 0;
        }
        epfd = epoll_create(1);
        if (epfd < 0)
        {
            perror("epoll_create()");
            return;
        }

        bzero(&ev_poll_chr, sizeof(struct epoll_event));
        ev_poll_chr.events = EPOLLIN | EPOLLPRI;

        err = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev_poll_chr);
        if (err < 0)
        {
            perror("epoll_ctl1 error");
            return 0;
        }
        err = epoll_wait(epfd, &ev_poll_chr, 1, 10000);
        if (err < 0)
            perror("epoll_wait error");
        else if (err == 0)
            printf("timeout\n");
        else
            printf("not empty\n");

        err = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev_poll_chr);
        if (err < 0)
            perror("epoll_ctl2 error");
    }
    else
        printf("open error\n");
    return 0;
}