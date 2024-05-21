# 等待队列

Linux内核的等待队列（Wait Queue）是重要的数据结构，与进程调度机制紧密相关联，可以用来同步对系统资源的访问、异步事件通知、跨进程通信等。
在Linux中，等待队列以循环链表为基础结构，包括两种数据结构：等待队列头(wait queue head)和等待队列元素(wait queue)，整个等待队列由等待队列头进行管理。

## 使用

```c
#include <linux/wait.h>

// 定义等待队列头
wait_queue_head_t wq;
// 初始化
init_waitqueue_head(&wq);

/*上面两步可以直接使用下面宏完成*/
DECLARE_WAIT_QUEUE_HEAD(wq);

// 定义等待队列元素
DECLARE_WAITQUEUE(name, tsk);
// 一般将当前进程定义成名为wait的等待队列元素
DECLARE_WAITQUEUE(wait, current);

// 添加/移除等待队列
add_wait_queue(&wq, &wait);
remove_wait_queue(&wq, &wait);

// 等待事件
wait_event(queue, condition);
wait_event_interruptible(queue, condition);
wait_evect_timeout(queue, condition, timeout)
wait_event_interruptible_timeout(queue, condition, timeout);
// queue 是等待队列头，condition 是等待条件，timeout 是超时时间
// _interruptible可以被信号打断，_timeout意味阻塞等待的超时时间

// 唤醒队列
// wq为上面定义的等待队列头
wake_up(&wq);
wake_up_interruptible(&wq);

```