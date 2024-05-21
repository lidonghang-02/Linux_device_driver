# 轮询操作
> 关于用户空间的轮询操作，poll，select，epoll等自行查询，这里只简单介绍设备驱动中的poll函数


poll函数一个典型模板：
```c
unsigned int poll_chr_poll_func(struct file* filp, poll_table* wait)
{
    unsigned int mask = 0;
    // 获取设备结构体指针
    struct xxx_dev* dev = filp->private_data;

    ...
    // 分别加入读写等待队列
    poll_wait(filp, &dev->r_wq, wait);
    poll_wait(filp, &dev->w_wq, wait);

    // 可读
    if (...)
        mask |= POLLIN | POLLRDNORM;
    // 可写
    if (...)
        mask |= POLLOUT | POLLWRNORM;

  return mask;
}
```

这个函数主要进行下面两项工作
1. 对可能引起设备文件状态变化的等待队列调用`poll_wair()`函数，将对应的等待队列头添加到`poll_table`结构体中
2. 返回表示是否能对设备进行无阻赛的读、写访问的掩码