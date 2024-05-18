#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#define DEVICE_NAME "spinlock_dev"

// static struct cdev cdev;

#define SPINLOCK_MAJOR 232
static int spinlock_major = SPINLOCK_MAJOR;

struct spinlock_dev
{
    struct cdev cdev;
    int shared_resource;
};

struct spinlock_dev *spinlock_devp;
struct class *my_class;
struct device *class_dev = NULL;
static dev_t dev_no;

// 自旋锁
spinlock_t my_lock;

static int
spinlock_open(struct inode *inode, struct file *file)
{
    file->private_data = spinlock_devp;
    printk(KERN_INFO "Spinlock Device Opened\n");
    return 0;
}
static int spinlock_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Spinlock Device Closed\n");
    return 0;
}
static ssize_t spinlock_read(struct file *filp, char __user *buf, size_t size,
                             loff_t *ppos)
{
    struct spinlock_dev *dev = filp->private_data;

    // 从用户空间读取数据
    if (copy_to_user(buf, &dev->shared_resource, sizeof(int)))
    {
        printk(KERN_ERR "Failed to copy data to user space\n");
        return -EFAULT;
    }
    else
        printk(KERN_INFO "shared_resource: %d\n", dev->shared_resource);

    ppos += sizeof(int);

    return 0;
}
static ssize_t spinlock_write(struct file *filp, const char __user *buf, size_t size,
                              loff_t *ppos)
{
    unsigned long flags;
    unsigned int count = size;
    struct spinlock_dev *dev = filp->private_data;

    // 加锁
    spin_lock_irqsave(&my_lock, flags);

    // 假设的共享资源操作
    dev->shared_resource += 1;

    // 解锁
    spin_unlock_irqrestore(&my_lock, flags);

    printk(KERN_INFO "shared_resource: %d\n", dev->shared_resource);

    return count;
}

// 设备操作结构体
static const struct file_operations fops = {
    .open = spinlock_open,
    .release = spinlock_release,
    .read = spinlock_read,
    .write = spinlock_write,
};

// 驱动初始化函数
static int __init my_driver_init(void)
{
    spin_lock_init(&my_lock);

    int ret;
    dev_no = MKDEV(spinlock_major, 0);

    // 分配设备号
    if (spinlock_major)
        ret = register_chrdev_region(dev_no, 1, DEVICE_NAME);
    else
    {
        ret = alloc_chrdev_region(&dev_no, 0, 1, DEVICE_NAME);
        spinlock_major = MAJOR(dev_no);
    }
    if (ret < 0)
    {
        printk(KERN_ERR "Failed to allocate device number\n");
        return ret;
    }
    spinlock_devp = kzalloc(sizeof(struct spinlock_dev), GFP_KERNEL);
    if (!spinlock_devp)
    {
        ret = -ENOMEM;
        goto free_chrdev_region;
    }

    // 初始化字符设备
    cdev_init(&spinlock_devp->cdev, &fops);
    spinlock_devp->cdev.owner = THIS_MODULE;
    ret = cdev_add(&spinlock_devp->cdev, dev_no, 1);
    if (ret < 0)
    {
        printk(KERN_ERR "Failed to add cdev\n");
        goto free_chrdev_region;
    }

    printk(KERN_INFO "My Spinlock Driver Init\n");

    return 0;

free_chrdev_class:
    class_destroy(my_class);

free_chrdev_region:
    unregister_chrdev_region(dev_no, 1);

    return ret;
}

// 驱动退出函数
static void __exit my_driver_exit(void)
{
    cdev_del(&spinlock_devp->cdev);
    unregister_chrdev_region(dev_no, 1);
    kfree(spinlock_devp);

    printk(KERN_INFO "My Spinlock Driver Exited\n");
}

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lidonghang-02");
MODULE_DESCRIPTION("Simple spinlock example with char device");