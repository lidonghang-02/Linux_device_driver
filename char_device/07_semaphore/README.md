# 信号量
对应操作系统中的经典概念PV操作，信号量的值可以是0、1或n。
## 使用

```c
#include <linux/semaphore.h>

// 定义
struct semaphore sem;

// 初始化信号量sem的值为val
sema_init(&sem, int val);

// 获得信号量
// 会导致当前进程睡眠，不能被信号打断
down(&sem);
// 会导致当前进程睡眠，可被信号打断
down_interruptible(&sem);

// 释放
up(&sem);
```

## 运行

```bash
make
insmod semaphore.ko

gcc test.c -o test
./test

// 另外开启一个终端
cat /dev/sem_dev
// 会发现只有当test结束后，才会输出内容
```

## 互斥锁
与信号量类似，但是互斥锁的值只能是0或1。
```c
#include <linux/mutex.h>

// 声明
struct mutex lock;

// 初始化
mutex_init(&lock);

// 获取互斥锁
mutex_lock(&lock);
mutex_lock_interruptible(&lock);
// 尝试获取互斥锁，失败时不会引起睡眠
mutex_trylock(&lock);

// 释放
mutex_unlock(&lock);

```