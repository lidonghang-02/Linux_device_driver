#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>

// 宏用于打印统计信息
#define PRT(a, b) pr_info("%-15s = %10d %10ld KB %8ld MB\n", \
                         a, b, (PAGE_SIZE * b) / 1024, (PAGE_SIZE * b) / 1024 / 1024)

static int __init my_init(void)
{
    struct page *p;
    unsigned long i, pfn, valid = 0;
    int free = 0, locked = 0, reserved = 0, swapcache = 0,
        referenced = 0, slab = 0, private = 0, uptodate = 0,
        dirty = 0, active = 0, writeback = 0, mappedtodisk = 0;

    // 获取物理页面的总数
    unsigned long num_physpages = get_num_physpages();

    // 遍历所有物理页面
    for (i = 0; i < num_physpages; i++) {
        // 计算页帧号，考虑体系结构相关的偏移
        pfn = i + ARCH_PFN_OFFSET;

        // 检查页帧号是否有效
        if (!pfn_valid(pfn))
            continue;
        // 有效的物理页面
        valid++;
        // 将页帧号转换为页面结构指针
        p = pfn_to_page(pfn);
        if (!p)
            continue;

        // 如果页面引用计数为0，则为空闲页面
        if (!page_count(p)) {
            free++;
            continue;
        }
        // 锁定页面
        if (PageLocked(p))
            locked++;
        // 保留页面
        if (PageReserved(p))
            reserved++;
        // 交换缓存页面
        if (PageSwapCache(p))
            swapcache++;
        // 被引用页面
        if (PageReferenced(p))
            referenced++;
        // Slab页面
        if (PageSlab(p))
            slab++;
        // 私有页面
        if (PagePrivate(p))
            private++;
        // 最新数据页面
        if (PageUptodate(p))
            uptodate++;
        // 脏页面
        if (PageDirty(p))
            dirty++;
        // 活跃页面
        if (PageActive(p))
            active++;
        // 写回页面
        if (PageWriteback(p))
            writeback++;
        // 映射到磁盘的页面
        if (PageMappedToDisk(p))
            mappedtodisk++;
    }

    // 输出统计信息
    pr_info("%-15s = %10s %10s(KB) %8s(MB)\n","数量","内存大小","内存大小");
    PRT("Total", num_physpages);
    PRT("Valid", valid);
    PRT("Free", free);
    PRT("Locked", locked);
    PRT("Reserved", reserved);
    PRT("SwapCache", swapcache);
    PRT("Referenced", referenced);
    PRT("Slab", slab);
    PRT("Private", private);
    PRT("Uptodate", uptodate);
    PRT("Dirty", dirty);
    PRT("Active", active);
    PRT("Writeback", writeback);
    PRT("MappedToDisk", mappedtodisk);

    return 0;
}

static void __exit my_exit(void)
{
    pr_info("Module exit.\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lidonghang-02");


