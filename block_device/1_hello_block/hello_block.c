#include <linux/module.h>
#include <linux/blkdev.h>

#define HELLO_BLKDEV_NAME "hello_block"
#define HELLO_BLKDEV_MAJOR 0

#define DISK_SECTOR_SIZE 512                       // 每扇区大小
#define DISK_SECTOR 1024                           // 总扇区数，
#define DISK_SIZE (DISK_SECTOR_SIZE * DISK_SECTOR) // 总大小，共0.5M
#define DISK_MINORS 16

#define KERNEL_SECTOR_SIZE 512

static int hello_blkdev_major = HELLO_BLKDEV_MAJOR;
module_param(hello_blkdev_major, int, 0);

struct gendisk *gd;
struct request_queue *queue;
unsigned char data[DISK_SIZE];
spinlock_t lock;

static void hello_blk_request(struct request_queue *q)
{
    struct request *req; // 正在处理的请求队列中的请求
    struct bio *bio;     // 当前请求的bio
    struct bio_vec bvec; // 当前请求的bio的段(segment)链表
    char *buffer;        // 磁盘块设备的请求在内存中的缓冲区
    unsigned long offset;
    unsigned long nbytes;
    sector_t sector;
    struct bvec_iter iter;
    // int i = 0;

    while ((req = blk_fetch_request(q)) != NULL)
    {
        if (req->cmd_type != REQ_TYPE_FS)
        {
            printk(KERN_NOTICE "Skip non-fs request\n");
            __blk_end_request_all(req, -EIO);
            continue;
        }
        __rq_for_each_bio(bio, req)
        {
            sector = bio->bi_iter.bi_sector;
            bio_for_each_segment(bvec, bio, iter)
            {
                // 下面这个函数在新版内核中已经删除
                buffer = __bio_kmap_atomic(bio, iter);
                offset = sector * KERNEL_SECTOR_SIZE;
                nbytes = (bio_cur_bytes(bio) >> 9) * KERNEL_SECTOR_SIZE;
                if ((offset + nbytes) > DISK_SIZE)
                    printk(KERN_NOTICE "Beyond-end write (%ld %ld)\n", offset, nbytes);
                else
                {
                    if (rq_data_dir(req) == WRITE)
                        memcpy(data + offset, buffer, nbytes);
                    else
                        memcpy(buffer, data + offset, nbytes);
                }

                sector += (bio_cur_bytes(bio) >> 9);
                __bio_kunmap_atomic(buffer);
            }
        }
        __blk_end_request_all(req, 0);
    }

    // 下面内容是另一种方法

    // while ((req = blk_fetch_request(q)) != NULL)
    // {
    //     if (req->cmd_type != REQ_TYPE_FS)
    //     {
    //         printk(KERN_NOTICE "Skip non-fs request\n");
    //         __blk_end_request_all(req, -EIO);
    //         continue;
    //     }
    //     bio = req->bio; // 获取当前请求的bio
    //     switch (rq_data_dir(req))
    //     {
    //         // 判断请求的类型，读还是写
    //     case READ:
    //         // 遍历request请求的bio链表
    //         while (bio != NULL)
    //         {
    //             sector = bio->bi_iter.bi_sector;
    //             // 　for循环处理bio结构中的bio_vec结构体数组（bio_vec结构体数组代表一个完整的缓冲区）
    //             for (i = 0; i < bio->bi_vcnt; i++)
    //             {
    //                 offset = sector * DISK_SECTOR_SIZE;
    //                 nbytes = (bio_cur_bytes(bio) >> 9) * DISK_SECTOR_SIZE;

    //                 bvec = bio->bi_io_vec[i];
    //                 buffer = __bio_kmap_atomic(bio, bio->bi_iter);

    //                 memcpy(buffer, data + offset, nbytes);
    //                 sector += bio_cur_bytes(bio) >> 9;
    //                 __bio_kunmap_atomic(buffer);
    //             }
    //             bio = bio->bi_next;
    //         }
    //         __blk_end_request_all(req, 0);
    //         break;
    //     case WRITE:
    //         // 遍历request请求的bio链表
    //         while (bio != NULL)
    //         {
    //             sector = bio->bi_iter.bi_sector;
    //             offset = sector * DISK_SECTOR_SIZE;
    //             nbytes = (bio_cur_bytes(bio) >> 9) * DISK_SECTOR_SIZE;
    //             // 　for循环处理bio结构中的bio_vec结构体数组（bio_vec结构体数组代表一个完整的缓冲区）
    //             for (i = 0; i < bio->bi_vcnt; i++)
    //             {
    //                 bvec = bio->bi_io_vec[i];
    //                 buffer = __bio_kmap_atomic(bio, bio->bi_iter);
    //                 memcpy(data + offset, buffer, nbytes);
    //                 // 将数据拷贝到内存中
    //                 sector += bio_cur_bytes(bio) >> 9;
    //                 __bio_kunmap_atomic(buffer);
    //             }
    //             bio = bio->bi_next;
    //         }
    //         __blk_end_request_all(req, 0);
    //         break;
    //     default:
    //         break;
    //     }
    // }
}

static int hello_blk_open_func(struct block_device *bdev, fmode_t mode)
{
    printk(KERN_INFO "hello_blk_open\n");
    return 0;
}

static void hello_blk_release_func(struct gendisk *gd, fmode_t mode)
{
    printk(KERN_INFO "hello_blk_release\n");
}

static int hello_blk_ioctl_func(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
    printk("ioctl cmd 0x%08x\n", cmd);
    return -ENOTTY;
}

static const struct block_device_operations hello_blk_fops = {
    .owner = THIS_MODULE,
    .open = hello_blk_open_func,
    .release = hello_blk_release_func,
    .ioctl = hello_blk_ioctl_func,
};

static int __init hello_blk_init_module(void)
{
    int ret;

    hello_blkdev_major = register_blkdev(hello_blkdev_major, HELLO_BLKDEV_NAME);
    if (hello_blkdev_major <= 0)
    {
        printk(KERN_ERR "Failed to get a major number.\n");
        return -EBUSY;
    }

    spin_lock_init(&lock);

    queue = blk_init_queue(hello_blk_request, &lock);
    if (!queue)
    {
        ret = -ENOMEM;
        goto out_1;
    }
    blk_queue_logical_block_size(queue, DISK_SECTOR_SIZE);

    // 注册一个磁盘
    gd = alloc_disk(DISK_MINORS);
    if (!gd)
    {
        printk(KERN_ERR "Failed to allocate gendisk structure.\n");
        ret = -ENOMEM;
        goto out_2;
    }

    strcpy(gd->disk_name, HELLO_BLKDEV_NAME);
    gd->major = hello_blkdev_major;
    gd->first_minor = 0;
    gd->fops = &hello_blk_fops;
    gd->queue = queue;
    set_capacity(gd, DISK_SIZE / KERNEL_SECTOR_SIZE);
    add_disk(gd);

    printk("module %s load SUCCESS...\n", HELLO_BLKDEV_NAME);
    return 0;

out_2:
    blk_cleanup_queue(queue);
out_1:
    unregister_blkdev(hello_blkdev_major, HELLO_BLKDEV_NAME);
    return ret;
}

static void __exit hello_blk_exit_module(void)
{
    del_gendisk(gd);
    put_disk(gd);
    blk_cleanup_queue(queue);
    unregister_blkdev(hello_blkdev_major, HELLO_BLKDEV_NAME);
}

module_init(hello_blk_init_module);
module_exit(hello_blk_exit_module);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");