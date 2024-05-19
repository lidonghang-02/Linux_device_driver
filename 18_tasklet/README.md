# tasklet

使用tasklet构建下面的数据表
```
   time   delta  inirq    pid   cpu command
  1321555    0     0     10659   3   cat
  1321805  250     1       0     3  swapper/3
  1322055  250     1       0     3  swapper/3
  1322306  251     1       0     3  swapper/3
  1322560  254     1       0     3  swapper/3

```

## linux中断处理

对于中断处理，linux将其分为两个部分——上半部（top half）和下半部（bottom half）。上半部用于完成尽量少的比较紧急的任务并在清除中断标志后进行“登记中断”的工作，即将下半部处理程序挂到该设备的下半部执行队列中。

一般来说，上半部被设计成不可中断，下半部可以被新的中断打断。下半部几乎做了中断处理程序的所有工作，下半部相对来说不是非常紧急的工作，而且相对耗时，不在硬件中断服务程序中执行。

## 实现下半部的机制——tasklet
> 除了tasklet以外，实现下半部的机制还有工作队列（workqueue）、软中断和线程化irq

> `tasklet`的使用比较简单，他的执行上下文软中断，我们只需要定义`tasklet`及其处理函数，并将二者关联起来即可。

### tasklet_struct 结构体

```c
struct tasklet_struct {
	struct tasklet_struct *next;     //链表中的下一个tasklet
	unsigned long state;             //tasklet的状态        
	atomic_t count;                  //引用计数器
	void(*func) (unsigned long data) //tasklet处理函数
	unsigned long data;              //给tasklet处理函数的参数     
}；
```
- state：表示该tasklet的状态，`TASKLET_STATE_SCHED`表示该tasklet以及被调度到某个CPU上执行，`TASKLET_STATE_RUN`表示该tasklet正在某个cpu上执行
- count：和enable或者disable该tasklet的状态相关，如果count等于0那么该tasklet是处于enable的，如果大于0，表示该tasklet是disable的。

### 初始化tasklet

1. 动态初始化
```c
void tasklet_init(struct tasklet_struct *t,void (*func)(unsigned long), unsigned long data)
```

2. 静态初始化
```c
DECLARE_TASKLET(name, func, data)
DECLARE_TASKLET_DISABLED(name, func, data)
```
这两个宏都可以静态定义一个struct tasklet_struct的变量，只不过初始化后的tasklet一个是处于eable状态，一个处于disable状态的。

### 调度tasklet

```c
void tasklet_schedule(struct tasklet_struct *t)
```

### 启用/禁止tasklet

```c
void tasklet_enable(struct tasklet_struct *t)
void tasklet_disable(struct tasklet_struct *t)
```

### 删除tasklet

```c
void tasklet_kill(struct tasklet_struct *t)
```

