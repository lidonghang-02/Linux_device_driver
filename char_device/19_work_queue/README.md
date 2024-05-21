# 工作队列
工作队列的使用方法和`tasklet`类似，但是工作队列的执行上下文是内核线程，可以调度和睡眠。

使用工作队列构造下面数据表，并通过设备读取
```
   time   delta  inirq    pid   cpu command
     8890    0     0      3813   1   cat
     8890    0     0       108   0   kworker/u14:4
     8890    0     0       108   0   kworker/u14:4
     8890    0     0       108   0   kworker/u14:4
     8890    0     0       108   0   kworker/u14:4
```

 
## struct work_struct
`struct work_struct` 是一个用来描述一个可延迟执行的工作的结构体。
使用 `INIT_WORK(_work, _func)` 宏可以初始化一个 work_struct 实例并与处理函数绑定。

## 工作队列API

### alloc_workqueue

> 创建一个新的工作队列并返回一个指向新创建的workqueue_struct结构的指针。
```c
struct workqueue_struct *alloc_workqueue(const char *name, int flags, int max_active,...);
```
- `name`:这是一个格式字符串，用于指定工作队列的名称。它可以包含%s、%d等格式化占位符，后跟相应的可变参数（...）。
- `flags`：用于配置工作队列的标志位。常见的标志包括：
    - `WQ_UNBOUND：工作队列中的工作可以在任何CPU上执行，而不是仅限于创建工作队列的CPU。`
    - `WQ_MEM_RECLAIM`：允许工作队列在内存不足时回收内存。
    - `WQ_HIGHPRI`：创建一个高优先级的工作队列，其中的工作会比其他工作队列中的工作更频繁地运行。
    - `WQ_CPU_INTENSIVE`：创建一个CPU密集型的工作队列，该队列中的工作会倾向于在相同的CPU上连续运行。
    - `WQ_FREEZABLE`：允许工作队列在冻结时暂停执行工作。
- `max_active`：工作队列中可以同时运行的最大工作数。如果设置为0，则没有限制。

### queue_work

这个函数用于将一个工作（work_struct）添加到工作队列中。当工作队列的调度器线程（或线程池中的某个线程）变得可用时，它会执行这个工作。这个函数接受一个指向workqueue_struct的指针和一个指向work_struct的指针作为参数。
```c
int queue_work(struct workqueue_struct *wq, struct work_struct *work);
```
### destroy_workqueue()

这个函数用于销毁一个工作队列。在销毁之前，它会确保队列中的所有工作都已完成。这个函数接受一个指向workqueue_struct结构的指针作为参数。

```c
void destroy_workqueue(struct workqueue_struct *wq);
```