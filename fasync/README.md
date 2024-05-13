# 异步通知

## 概念

异步通知：一旦设备就绪，则主动通知应用程序，这样应用程序就不需要查询设备状态，类似于“中断”。Linux中通过信号来通知应用程序。

## 信号的接收

在应用程序中，可以通过`signal()`函数来设置对应信号的处理函数。
```c
void (*signal)(int signum,void(*handler)(int))(int);
```
- 第一个参数指定信号的值，即信号类型
- 第二个参数指定信号的处理函数
  - 若是 **`SIG_IGN`** 表示忽略该信号
  - 若是 **`SIG_DFL`** 表示使用默认处理函数

- 如果`signal()`调用成功返回处理函数`handler`的值，失败返回`SIG_ERR`

## 信号的释放

在设备驱动和应用程序的异步通知交互中，仅仅在应用程序端捕获信号是不够的，因为信号的源头在设备驱动端，所以应该在合适的时机让设备驱动释放信号。

为了让设备驱动支持异步通知机制，驱动程序中涉及3项工作

1. 支持 **`F_SETOWN`** 命令，能在这个控制命令处理中设置`filp->f_owner`为对应进程ID（这项工作由内核完成，设备驱动无须处理）。
2. 支持 **`F_SETFL`** 命令的处理，每当 `FASYNC` 标志改变时，驱动程序中的 `fasyna()`函数将执行。所以驱动程序需要实现`fasyna()`函数。
3. 在设备资源可获得时，调用`kill_fasync()`函数激发相应的信号。

驱动中的上面3项工作和应用程序中的是一一对应的

| 用户空间 | 驱动空间 |
| --------- | --------- |
| `fcntl(fd, F_SETOWN, getpid())` | 内核设置`filp->f_owner` |
| `fcntl(fd,F_GETFL)` | 设备驱动`fasync()`函数 |
| `signal()`函数绑定信号和处理函数 | 资源可获得时，调用`kill_fasync()`函数释放信号 | 

## 设备驱动中的异步通知编写

1. 处理`FASYNC`标志变更的函数
```c
int fasync_helper(int fd, struct file *filp, int mode, struct fasync_struct **fa);
```

2. 释放信号用的函数

```c
void kill_fasync(stryct fasync_struct *fa, int sig, int band);
```

3. 模板
```c
struct xxx_dev{
    struct cdev cdev;
    ...
    struct fasync_struct *fasync_queue; // 异步结构体指针
};

static int xxx_fasync(int fd,struct file *filp,int mode)
{
    struct xxx_dev *dev = filp->private_data;
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static ssize_t xxx_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos)
{
    struct xxx_dev *dev = filp->private_data;
    ...
    // 激发信号
    if(dev->fasync_queue)
        kill_fasync(dev->fasync_queue, SIGIO, POLL_IN);
    ...
}

static int xxx_release(struct inode *inode,struct file *filp)
{
    // 将文件从异步通知列表中删除
    xxx_fasync(-1,filp,0);
    ...
    return 0;
}

```

## 测试

```bash
dmesg -C

make
gcc test_fasync.c -o test_fasync

insmod fasync.ko
./test_fasync &

echo "hello world" > /dev/fasync0
cat /dev/fasync0

dmesg

rmmod fasync.ko

```