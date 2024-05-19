# 定时器

## timer_list结构体

Linux内核中的定时器是通过timer_list结构体来表示的。该结构体包含了一些字段，用于记录定时器的到期时间、回调函数等。

```c
struct timer_list {
    ...
    unsigned long expires;
    void (*function)(unsigned long);
    unsigned long data;
    ...
};
```
- expires字段记录定时器的到期时间（以jiffies为单位）
- function字段指向定时器的回调函数
- data字段用于传递给回调函数的数据。


## 函数
### init_timer()
init_timer是一个宏，用于初始化一个timer_list结构体实例。
```c
void init_timer(struct timer_list *timer);
```
`init_timer()`只会初始化`timer_list`的`entry`为`NULL`，并给bash指针赋值。

> 初始化定时器也可以使用`DEFINE_TIMER`宏和`setup_timer`函数

### add_timer()
add_timer函数用于向Linux内核注册一个定时器。
```c
void add_timer(struct timer_list *timer);
```

### del_timer()
del_timer函数用于从Linux内核中注销一个定时器。
```c
void del_timer(struct timer_list *timer);
```
`del_timer_sync()`是`del_timer()`的同步版本，它等待定时器其被处理完后删除，因此该函数不能发生在中断上下文中。


### mod_timer：

mod_timer函数用于修改已注册的定时器的到期时间。

```c
int mod_timer(struct timer_list *timer, unsigned long expires);
```
其中，timer参数是要修改到期时间的定时器，expires参数是新的到期时间（以jiffies为单位）。如果定时器还没有被激活（即还没有到达其原始到期时间），mod_timer函数会激活该定时器。

mod_timer函数的返回值表示在调用该函数之前定时器的状态：如果返回0，表示定时器在调用前未被激活；如果返回1，表示定时器在调用前已被激活。

