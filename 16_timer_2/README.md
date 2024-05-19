# 定时器的应用

使用定时器构造下面的数据表

```
   time   delta  inirq    pid   cpu command
  1321555    0     0     10659   3   cat
  1321805  250     1       0     3  swapper/3
  1322055  250     1       0     3  swapper/3
  1322306  251     1       0     3  swapper/3
  1322560  254     1       0     3  swapper/3

```

## 部分用到的函数

### setup_timer()
setup_timer 是 Linux 内核中用于初始化一个 struct timer_list 结构体的函数。这个结构体用于表示一个内核定时器，当定时器到期时，会调用一个预先设定的函数（称为定时器处理函数）。

```c
void setup_timer(struct timer_list *timer, void (*function)(struct timer_list *), unsigned int flags);
```
参数说明：
- struct timer_list *timer：指向要初始化的 timer_list 结构体的指针。
- void (*function)(struct timer_list *)：当定时器到期时调用的处理函数。这个函数应该接受一个指向 timer_list 结构体的指针作为参数。
- unsigned int flags：标志位，用于控制定时器的行为。通常设置为 0，但也可以包含一些特殊的标志位，如 TIMER_DEFERRABLE（表示该定时器可以延迟执行）。

在初始化定时器之后，你可以使用 mod_timer 或 add_timer 函数将其添加到内核的定时器队列中，以便在指定的时间后触发。


### in_interrupt()

in_interrupt() 是一个宏，它用于检查当前是否处于中断上下文中。

### current

current 是一个指向当前正在运行的进程描述符（通常是task_struct类型）的指针。
- pid 是该进程描述符中的一个成员，表示进程的进程ID（PID）。
- comm 是task_struct结构中的一个成员，它表示进程的命令名。
### smp_processor_id()
smp_processor_id() 是一个宏，用于在多处理器系统（SMP）上获取当前CPU的ID。在多处理器系统中，每个CPU都有一个唯一的ID，这个ID可以通过这个宏来获取。
