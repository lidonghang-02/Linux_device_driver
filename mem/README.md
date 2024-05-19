# 内核空间内存动态申请

## kmalloc()

函数原型：
```c
void *kmalloc(size_t size, gfp_t flags);
```
参数说明：
- size：要分配的内存块的大小，以字节为单位。
- flags：分配标志，用于指定内存分配的策略和属性。
  - GFP_KERNEL：在内核空间的进程中分配内存
  - GFP_ATOMIC：在原子上下文中分配内存

`kmalloc()`的底层依赖于`__get_free_page()`来实现，当使用 `GFP_KERNEL` 申请内存时，若暂时不能满足，则进程会睡眠等待页，引起阻塞，因此不能在 **中断上下文** 或持有 **自选锁** 的时候使用`GFP_KERNEL`申请内存。对于这些在非进程上下文中不能阻塞的情况，应当使用`GFP_ATOMIC`申请内存，若不存在空闲页，则不等待，直接返回。

和`kmalloc()`类似，**`kzalloc()`** 函数也用于分配内存，区别在于`kzalloc()`函数会在分配的内存块中清零。

`kmalloc()`和`kzalloc()`申请的内存都使用 **`kfree()`** 函数释放

## __get_free_pages()

`__get_free_pages()`系列函数/宏本质上是Linux内核最底层用户获取空闲内存的方法。因为底层的buddy算法以2^n页的方式管理内存，所以底层申请内存是以2^n页为单位。n最大为10或11

- `__get_free_pages(gfp_mask, int order)`：分配2^order页的内存。
- `__get_free_page(gfp_mask)`：分配一个页（4KB）的内存。
  - 这个宏实际上就是调用`__get_free_pages`分配1页内存
- `get_zeroed_page(gfp_mask)`：分配并清零一个页的内存。


在可能阻塞的上下文中（如进程上下文），应使用允许阻塞的标志（`GFP_KERNEL`）。在不允许阻塞的上下文中（如中断上下文或持有自旋锁时），应使用不允许阻塞的标志（`GFP_ATOMIC`或`GFP_NOWAIT`）。

上面的申请的内存使用`free_page(addr)`或`free_pages(addr, order)`释放。

## vmalloc()

`vmalloc()`函数用于在内核中分配大的、连续的虚拟地址空间，但物理地址可能不连续。这通常用于需要连续虚拟地址但不需要物理连续性的情况，如大型数据结构或设备驱动程序的缓冲区。`vmalloc()`需要建立新的页表项，所以其开销很大。此外，`vmalloc()`不能应用在原子上下文中，因为其内部实现使用了`GFP_KERNEL`标志的`kmalloc()`。

函数原型：
```c
void *vmalloc(unsigned long size);
void vfree(void *addr);
```

## slab分配器

在Linux中，伙伴分配器（buddy allocator）是以页为单位管理和分配内存。但在内核中的需求，一方面，完全使用页为单位申请内存严重浪费内存;另一方面，Linux运行过程中经常会涉及到大量重复对象的重复生成、使用和释放内存问题（task_struct、inode等）。

为了解决这个问题，Linux内核引入了slab分配器。slab分配器以字节为单位，从 Buddy 分配器中申请内存，之后对申请来的内存细分管理。

### struct kmem_cache

struct kmem_cache：这是Slab分配器的一部分，它提供了一种用于快速分配和释放固定大小内存块的机制。Slab分配器为每种大小的对象维护一个缓存池，从而提高了内存分配的效率。

### kmem_cache_create()
> 创建一个新的kmem_cache实例。

函数原型：
```c
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align, unsigned long flags, void (*ctor)(void *));
```
参数说明:
- name: 缓存的名称，用于调试和日志记录。
- size: 缓存中每个对象的大小（以字节为单位）。
- align: 每个对象的对齐要求（通常设置为0，让slab分配器选择最佳的对齐）。
- flags: 标志位，用于控制缓存的行为
  - SLAB_HWCACHE_ALIGN：表示对象应该按硬件缓存行对齐，即对齐到一个缓存行
  - SLAB_RECLAIM_DMA：表示在DMA区域中分配
- ctor: 构造函数（可选），当从缓存中分配对象时，此函数将被调用以初始化对象。

返回值:

- 成功时返回一个指向新创建的 kmem_cache 结构体的指针。
- 失败时返回 NULL。

### kmem_cache_alloc()
> 从指定的kmem_cache中分配内存

函数原型:
```
c
void *kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags);
```
参数说明:
- cachep: 之前通过 kmem_cache_create() 创建的 kmem_cache 结构体的指针。
- flags: GFP（Get Free Pages）标志，用于控制内存分配的行为。

返回值:

- 成功时返回一个指向新分配对象的指针。
- 失败时（例如，由于内存不足）返回 NULL。

### kmem_cache_free()
> 释放`kmem_cache_alloc`申请的内存

函数原型:
```c
void kmem_cache_free(struct kmem_cache *cachep, void *objp);
```
参数说明:
- cachep: 与对象关联的 kmem_cache 结构体的指针。
- objp: 要释放的对象的指针。


### kmem_cache_destroy()
> 销毁一个kmem_cache实例。

函数原型:
```c
void kmem_cache_destroy(struct kmem_cache *cachep);
```

