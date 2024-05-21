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

static void hello_make_request(struct request_queue *q, struct bio *bio)
{
    struct bio_vec bvec;
    struct bvec_iter iter;
    char *buffer;
    unsigned long offset;

    sector_t sector = bio->bi_iter.bi_sector;
    unsigned long nbytes = (bio_cur_bytes(bio) >> 9) * KERNEL_SECTOR_SIZE;

    bio_for_each_segment(bvec, bio, iter)
    {
        offset = sector * KERNEL_SECTOR_SIZE;
        buffer = __bio_kmap_atomic(bio, iter);

        if (offset + nbytes > DISK_SIZE)
            printk(KERN_NOTICE "Beyond-end write (%ld %ld)\n", offset, nbytes);
        else
        {
            if (bio_data_dir(bio) == READ)
                memcpy(buffer, data + offset, nbytes);
            else
                memcpy(data + offset, buffer, nbytes);
        }
        sector += bio_cur_bytes(bio) >> 9;
        __bio_kunmap_atomic(buffer);
    }

    bio_endio(bio, 0);
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

    queue = blk_alloc_queue(GFP_KERNEL);
    blk_queue_make_request(queue, hello_make_request);

    // 告知该请求队列的逻辑块大小；
    blk_queue_logical_block_size(queue, DISK_SECTOR_SIZE);

    // 注册一个磁盘
    gd = alloc_disk(DISK_MINORS);
    if (!gd)
    {
        printk(KERN_ERR "Failed to allocate gendisk structure.\n");
        ret = -ENOMEM;
        goto out;
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

out:
    unregister_blkdev(hello_blkdev_major, HELLO_BLKDEV_NAME);
    blk_cleanup_queue(queue);
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