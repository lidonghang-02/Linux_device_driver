# 自旋锁（spinlock）

## 概念
Spinlock是linux内核中常用的一种互斥锁机制，和mutex不同，当无法持锁进入临界区的时候，当前执行线索不会阻塞，而是不断的自旋等待该锁释放。 正因为如此，自旋锁也是可以用在中断上下文的。 也正是因为自旋，临界区的代码要求尽量的精简，否则在高竞争场景下会浪费宝贵的CPU资源。

## 使用
```c
#include <linux/spinlock.h>
// 定义自选锁
spinlock_t lock;
// 初始化自旋锁
spin_lock_init(&lock);
// 获取自旋锁
spin_lock(&lock);
// 释放自旋锁
spin_unlock(&lock);
```

尽管使用自旋锁可以保证临界区不受别的CPU和本CPU内的抢占进程打扰。但是得到锁的代码路径在执行临界区的时候，可能受到 **中断** 的影响。因此，需要使用自旋锁的衍生函数。
```
// 锁 + 开/关中断
spin_lock_irq();
spin_unlock_irq();

// 锁 + 关中断并保存中断状态/开中断并恢复中断状态
spin_lock_irqsave();
spin_unlock_irqrestore();
```

## 测试
```bash
make
dmesg -C
insmod spinlock.ko
mknod /dev/spinlock c 232 0

cat /dev/spinlock
dmesg
echo "1" > /dev/spinlock
dmesg
cat /dev/spinlock
dmesg
```

