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

    // 打开/dev/timer0设备文件
    fd = open("/dev/timer0", O_RDONLY);
    if (fd != -1)
    {
        while (1)
        {
            read(fd, &counter, sizeof(unsigned int));
            if (counter != old_counter)
            {
                printf("/dev/timer0 :%d\n", counter);
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
