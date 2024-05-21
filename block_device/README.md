# Linux块设备驱动

> 块设备驱动比字符设备驱动复杂得多，这里只是简单介绍
> 这部分代码并没有严格测试，所以可能存在一些问题

## 目录

- 1_hello_block ：有IO调度器
- 2_hello_noIO ： 无IO调度器
- vmem_disk ： 这部分代码来自宋宝华老师的《Linux设备驱动开发详解》中的代码

## 概念
Linux块设备驱动是Linux内核中用于管理硬盘、闪存、USB存储设备等块设备的软件组件。块设备是指可以随机访问的数据存储设备，它们将数据划分为固定大小的块（或扇区）进行读写。与字符设备（如串口、终端）不同，块设备通常用于存储文件系统。

## 块设备的I/O操作特点

- 块设备的I/O操作特点与字符设备不同，块设备通常以固定大小的块进行读写操作
- 块设备使用请求队列，缓存并重排读写数据块的请求，用高效的方式读取数据；块设备的每个设备都关联了请求队列；对块设备的读写请求不会立即执行，这些请求会汇总起来，经过协同之后传输到设备
- 块设备可以随机访问，字符设备只能被顺序读写

## 块设备核心结构
- gendisk是一个磁盘或分区在内核中的描述。 
- block_device_operations描述磁盘的操作方法集。 
- request_queue每一个 gendisk 对象都有一个request_queue对象，表示针对一个gendisk对象的所有请求的队列。 
- request表示经过IO调度之后的针对一个gendisk(磁盘)的一个“请求"，是request_queue的一个节点。多个request 构成了一个request_queue。 
- bio描述符，通用块的核心数据结构，描述了块设备的I/O操作。 
- bio_vec描述bio中的每个段，多个bio_vec形成一个bio，多个bio经过IO调度和合并之后可以形成一个request。

![块设备结构](../pic/image.png)

## block_device_operations

与字符设备的file_operations类似，块设备也有一个操作函数集block_device_operations，用于定义块设备驱动的函数。我们要做的就是实现这个操作函数集，然后将其注册到内核中。

```c
#include <linux/blkdev.h>

struct block_device_operations {
    int (*open) (struct block_device *, fmode_t);
    void (*release) (struct gendisk *, fmode_t);
    int (*rw_page)(struct block_device *, sector_t, struct page *, bool);
    int (*ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
    int (*compat_ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
	...
    /* ->media_changed() is DEPRECATED, use ->check_events() instead */
    int (*media_changed) (struct gendisk *);
    int (*revalidate_disk) (struct gendisk *);
    int (*getgeo)(struct block_device *, struct hd_geometry *);
	...
};
```

- 和字符设备驱动不同，块设备驱动的block_device_operations操作集中没有负责读和写数据的函数；在块设备驱动中，这些操作是由request函数处理的。
- compat_ioctl：兼容性ioctl，用于处理32位系统下的ioctl调用。
- media_changed：用于检测驱动器中的介质是否改变。仅适用于支持可移动介质的驱动器。
  - 这个函数目前已被弃用，应使用`check_events()`替代。`media_changed()`在用户空间轮询可移动磁盘介质是否存在，新的`check_events()`在内核空间轮询。
- revalidate_disk：响应一个介质的该面，让驱动进行必要的工作以使新介质准备好。
- getgeo：用于获取磁盘的geometry信息，填充hd_geometry结构体。

## gendisk结构体

在Linux内核中，使用gendisk（通用磁盘）来表示一个磁盘。

```c
//include/linux/genhd.h

struct gendisk {
    int major;                      /* major number of driver */
    int first_minor;
    int minors;                     /* maximum number of minors, =1 for
                                     * disks that can't be partitioned. */
    char disk_name[DISK_NAME_LEN];  /* name of major driver */
    char *(*devnode)(struct gendisk *gd, umode_t *mode);

    struct hd_struct part0;

    const struct block_device_operations *fops;
    struct request_queue *queue;
	...
};
```
- major、first_minor、minors：磁盘的主次设备号，同一个磁盘的各个分区共享一个主设备号
- disk_name：磁盘的名称，在/proc/partitions 和 sysfs 中表示该磁盘。
- devnode：用于获取磁盘的设备节点。
- fops：指向块设备驱动的block_device_operations操作函数集。
- queue：管理这个设备的I/O请求队列的指针。

### gendisk的操作函数

#### 分配gendisk
```c
struct gendisk *alloc_disk(int minors);
```
minors 是次设备号的数量，也就是磁盘分区的数量，这个值初始化后就不能修改。

#### 增加gendisk

```c
int add_disk(struct gendisk *disk);
```
注册磁盘设备，这个函数必须在驱动程序的初始化工作完成并能响应磁盘的请求之后调用。

#### 删除gendisk
```c
void del_gendisk(struct gendisk *disk);
```

#### gendisk引用计数
```c
struct kobject *get_disk(struct gendisk *disk);
void put_disk(struct gendisk *disk); 
```
一般不需要使用。

## bio结构体

bio（block I/O request）是Linux内核中用于描述一个I/O请求的数据结构，块I/O操作在页级粒度的底层描述。

```c
// include/linux/blk_types.h

struct bio {
    struct bio      *bi_next;   /* request queue link */
    struct block_device *bi_bdev;
    int         bi_error;

    unsigned short      bi_flags;   /* status, command, etc */
    unsigned short      bi_ioprio;

    struct bvec_iter    bi_iter;

    atomic_t        __bi_remaining;
    bio_end_io_t        *bi_end_io;
    
    unsigned short      bi_vcnt;    /* how many bio_vec's */
    unsigned short      bi_max_vecs;    /* max bvl_vecs we can hold */
    struct bio_vec      *bi_io_vec; /* the actual vec list */
	......
}

```
### bio_vec结构体
块数据通过bio_vec结构体数组在内部被表示成I/O向量；每个bio_vec数组元素由三元组组成（即，页、页偏移、长度），表示该块I/O的一个段；

```c
// include/linux/bvec.h
struct bio_vec {
    struct page *bv_page;	// 页指针
    unsigned int    bv_len;	// 传输的字节数
    unsigned int    bv_offset;	// 偏移位置
};
```
### bvec_iter结构体
struct bvec_iter结构体用来记录当前bvec被处理的情况，用于遍历bio；

// include/linux/bvec.h
struct bvec_iter {
    sector_t        bi_sector;  /* device address in 512 byte sectors */
    unsigned int	bi_size;    /* residual I/O count */
    unsigned int	bi_idx;     /* current index into bvl_vec */
    unsigned int	bi_bvec_done;   /* number of bytes completed in current bvec */
};

## 请求(request)和请求队列(request_queue)

I/O调度算法将多个bio合并成一个请求，所以一个request包含多个bio。

每个块设备或块设备的分区都有对应的请求队列，每个请求队列单独执行I/O调度，请求队列是由请求结构实例链接成的双向链表，链表以及整个队列的信息用结构request_queue描述，称为请求队列对象结构或请求队列结构。

从I/O调度器合并和排序出来的请求会被分发到设备级的request_queue中，然后被分发到设备驱动程序中。

目前上面提到的所有结构体的总结可以用下表表示：

| 结构体 | 描述 |
| ---- | ---- |
| bio | 描述一个I/O请求，一个bio由多个bio_vec组成，多个bio经过I/O调度和合并形成一个request |
| bio_vec | 描述一个bio请求对应的所有内存 |
| bvec_iter | 描述一个bio_vec结构中的一个sector信息 |
| request | 描述经过I/O调度之后的针对gendisk的一个请求，是request_queue结构队列的一个节点，多个request构成了一个request_queue队列 |
| request_queue | 描述一个gendisk对象的所有请求的队列，是对应gendisk结构的一个成员 |
| block_device_operations | 描述一个块设备的操作函数集 |
| gendisk | 一个磁盘或分区在内核中的描述 |

![块设备结构](../pic/image.png)

### 请求队列函数

#### 定义一个请求队列
```c
static struct request_queue *my_block_queue; // 指向块设备请求队列的指针
```
#### 初始化请求队列

> **这个函数在较新版本的内核中已经弃用，可以通过下面`分配请求队列`中的方法初始化请求队列**
> 这个函数会使用IO调度器，即使用请求队列
```c
struct request_queue *blk_init_queue(request_fn_proc *rfn, spinlock_t *lock);
```
- rfn: 请求处理函数的指针
- lock: 控制访问队列权限的自旋锁

处理函数rfn的类型
```c
typedef int (request_fn_proc)(struct request_queue *q);
```

#### 清除请求队列

```c
void blk_cleanup_queue(struct request_queue *q);
```
- q: 一个指向 struct request_queue 结构体的指针，表示要清理的请求队列。它将会清空指定请求队列中的所有请求，并释放相关的资源。


#### 分配请求队列

> 这个函数不会使用IO调度器，即不会使用请求队列，所以需要指定“制造请求”函数
>
> 第一个参数仍然是“请求队列”，但实际上这个队列中没有任何请求，因为块层不需要将bio调整为请求
```c
struct request_queue *blk_alloc_queue(int gfp_mask);
void blk_queue_make_request(struct request_queue *q, make_request_fn *mfn);
```
- gfp_mask: 这是一个 GFP（Get Free Pages）标志，用于指定内存分配的类型和属性。常见的标志如 GFP_KERNEL 用于从内核内存池分配内存。
- mfn: 请求处理函数的指针

处理函数的类型：
```c
typedef void (make_request_fn)(struct request_queue *q, struct bio *bio);
```


#### 提取请求

```c
struct request *blk_peek_requesk(struct request_queue *q)
```
这个函数用于返回下一个要处理的请求（I/O调度器决定）， 如果队列中没有待处理的请求，则该函数返回 NULL。

这个函数不会清除请求，而是仍然保存在队列上。

获取到下一个要处理的请求以后就要开始处理这个请求

#### 启动请求

```c
void blk_start_request(struct request *req)
```
- req：上面blk_peek_requesk函数获取的请求

##### 对于上面两个函数我们可以使用下面函数同时完成

```c
struct request *blk_fetch_request(struct request_queue *q)
```

#### 遍历bio和片段

使用下面宏遍历一个请求的所有bio

```c
#define __rq_for_each_bio(_bio, _rq) \  
    if ((_rq->bio)) \  
        for (_bio = (struct bio *)(_rq)->bio; _bio; _bio = _bio->bi_next)
```

#### 报告完成
```c
void __blk_end_request_all(struct request *req, int error);
void blk_end_request_all(struct request *req, int error);
```
- req: 指向请求的指针
- error: 0-成功，其他-失败

## 块设备驱动的初始化

```c
// 注册设备
// 为块设备驱动分配一个未使用的主设备号，并在/proc/devies中添加一个入口
// 返回主设备号，失败返回一个负值
// 这个函数是可选的，即可以不注册
int register_blkdev(unsigned int major, const char *name);

// 注册请求队列
blk_init_queue();

// 设置读写块的大小
blk_queue_logical_block_size(xxx_queue, size);

// 创建磁盘
alloc_disk()
// 需要对磁盘进行初始化，设置major、first_minor、
// fops、queue、private_data等。
// 同时，使用下面函数设置存储容量（单位是扇区）
set_capacity()

add_disk()
```

## 块设备驱动的卸载

```c
// 删除磁盘
del_gendisk()

// 删除gendisk设备的引用
put_disk()

// 删除请求队列
blk_cleanup_queue()

// 删除设备
unregister_blkdev()
```