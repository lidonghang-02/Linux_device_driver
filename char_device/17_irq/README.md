# linux中断处理

对于中断处理，linux将其分为两个部分——上半部（top half）和下半部（bottom half）。上半部用于完成尽量少的比较紧急的任务并在清除中断标志后进行“登记中断”的工作，即将下半部处理程序挂到该设备的下半部执行队列中。

一般来说，上半部被设计成不可中断，下半部可以被新的中断打断。下半部几乎做了中断处理程序的所有工作，下半部相对来说不是非常紧急的工作，而且相对耗时，不在硬件中断服务程序中执行。实现下半部的机制一般有tasklet，工作队列、软中断和线程化irq

## 中断编程
### 申请和释放中断
1. **申请irq**

```c
int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev);
```
参数说明：
- irq：中断号
- handler：中断处理函数（上半部）,dev参数将被传递给该函数
- flags：中断标志
  - IRQF_SHARED：共享中断
  - IRQF_DISABLED：禁止中断
  - IRQF_SAMPLE_RANDOM：随机数中断
  - IRQF_TIMER：定时器中断
  - IRQF_PROBE_SHARED：共享中断探测
- name：中断名称
- dev：要传递给中断服务程序的私有数据，一般是设备结构体指针或NULL

返回值：
- 成功：0
- 失败：
  - EINCAL：中断号无效或者处理函数指针为NULL
  - EBUSY：中断号已被占用且不能共享

上半部handler的类型`irq_handler_t`定义为：
```c
typedef irqreturn_t (*irq_handler_t)(int, void *);
typedef int irqreturn_t;
```

2. **释放irq**


```c
void free_irq(unsigned int irq, void *dev);
```

参数说明：
- irq：中断号
- dev：要传递给中断服务程序的私有数据，一般是设备结构体指针或NULL

### 使能和屏蔽中断
1. **使能中断**

```c
void enable_irq(unsigned int irq);
```
恢复**本CPU**内所有中断可以使用`local_irq_restore(flags)`宏和`local_irq_enable(void)`函数，前者会将目前的中断状态恢复，后者立即恢复中断。

2. **屏蔽中断**

```c
// 等待目前中断处理完成
void disable_irq(unsigned int irq);
// 立即返回
void disable_irq_nosync(unsigned int irq);
```

屏蔽**本CPU**内所有中断可以使用`local_irq_save(flags)`宏和`local_irq_disable(void)`函数，前者会将目前的中断状态保存在flags中(unsigned long类型)，后者直接禁止中断不保存状态。


