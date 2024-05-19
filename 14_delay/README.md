# 内核中的延时

## jiffies和HZ

### jiffies
 jiffies（系统时钟中断计数器）是一个全局变量，用于跟踪自系统启动以来经过的“滴答数”（tick counts）。每个“滴答”对应一个时钟中断的周期，这个周期的长度由内核配置中的 HZ 值决定。因此，jiffies 可以用来计算两个时间点之间的时间差，或者确定某个事件自上次发生以来已经过了多少时间。

由于 jiffies 是一个无符号整数，因此它有一个最大值（这取决于 jiffies 的数据类型和大小）。当 jiffies 达到其最大值并继续增加时，会发生“回绕”（wrap around），这意味着 jiffies 的值会重新从0开始计数。

### HZ
HZ 是一个内核配置参数，表示每秒的时钟中断次数。它定义了 jiffies 变量每秒增加的次数。传统的 HZ 值是100，意味着每秒有100个时钟中断，因此每个“滴答”是10毫秒。

## 延时函数

### jiffies实现延时
可通过比较两个jiffies来实现延时，比较使用的宏如下
```c
time_after(a,b)		    // a>b?		
time_before(a,b)	    // a<b?
time_after_eq(a,b)	    // a>=b?
time_before_eq(a,b)	    // a<=b?
```

jiffies与常用时间之间的转换

```c
jiffies_to_msecs()   // jiffies->s
jiffies_to_usecs()   // jiffies->us
msecs_to_jiffies()   // ms->jiffies
usecs_to_jiffies()   // us->jiffies

// 下面的宏是将jiffies转换为timespec64结构体
// 转换基于当前的HZ值，将jiffies表示的秒数和剩余的纳秒数
// 填充到timespec64结构体的tv_sec和tv_nsec字段中。
jiffies_to_timespec64()
// 与上一个函数相反，将timespec64结构体中的tv_sec和tv_nsec字段转换为jiffies表示的秒数和剩余的纳秒数。
timespec64_to_jiffies()
```

实现延时1000ms，这种方法是一种忙等待，浪费系统性能，有可能让系统进入死循环出不来（比如禁止了中断）

```c
unsigned long t = jiffies + msecs_to_jiffies(1000);
while(time_before(jiffies, t))
{}
```
### 等待队列实现延时
```c
// Linux 内核中用于等待某个条件满足或超时的宏

// 不会被信号打断
long wait_event_timeout(  
    wait_queue_head_t *wq,  
    int (*condition)(void *),  
    long timeout  
);
// 可以被信号打断
long wait_event_interruptible_timeout(  
    wait_queue_head_t *wq,        // 等待队列的头指针  
    int (*condition)(void *),     // 要等待的条件函数  
    long timeout                  // 超时时间（以 jiffies 为单位）  
);
```

使用wait_event_timeout函数实现延时1000ms
```c
wait_queue_head_t wait;         // 声明一个等待队列头
init_waitqueue_head(&wait);     // 初始化
// 上面两个函数可以简写为
// DECLARE_WAIT_QUEUE_HEAD(wait);

wait_event_interruptible_timeout(wait, 0, msecs_to_jiffies(1000));

``` 


### 内核中提供的短延时函数

```c
mdelay()    // ms
udelay()    // us
ndelay()    // ns
```

### 休眠延时
> 在等待的时间到来之前，进程处于睡眠状态，CPU资源被其他进程使用。

休眠延时函数
```c
schedule_timeout()
// 下面两个函数都是给予schedule_timeout()函数的实现
// 区别在于，前者设置进程状态为TASK_INTERRUPTIBLE，
// 后者设置为TASK_UNINTERRUPTIBLE
schedule_timeout_interruptible()
schedule_timeout_uninterruptible()

// 下面的函数都是基于上面函数的实现
msleep()
msleep_interruptible()
msleep_uninterruptible()
```
