#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/poll.h>

#include "fasync.h"

#define FASYNC_SIZE 0x10
#define FASYNC_MAJOR 0
#define FASYNC_MINOR 0
#define FASYNC_NR_DEVS 1

static int fasync_major = FASYNC_MAJOR;
static int fasync_minor = FASYNC_MINOR;
static int fasync_nr_devs = FASYNC_NR_DEVS;

static struct class *fasync_cls;
struct fasync_dev
{
    struct cdev cdev;
    struct device *class_dev;
    unsigned int len;
    unsigned char mem[FASYNC_SIZE];
    struct mutex mutex;
    wait_queue_head_t r_wq;
    wait_queue_head_t w_wq;

    struct fasync_struct *fasync_queue;
};

static struct fasync_dev *fasync_devp;

static int fasync_fasync(int fd, struct file *filp, int mode)
{
    struct fasync_dev *dev = filp->private_data;
    return fasync_helper(fd, filp, mode, &dev->fasync_queue);
}

static int fasync_open(struct inode *inode, struct file *filp)
{
    struct fasync_dev *dev = container_of(inode->i_cdev, struct fasync_dev, cdev);
    filp->private_data = dev;
    printk(KERN_INFO "fasync_open\n");
    return 0;
}

static int fasync_release(struct inode *inode, struct file *filp)
{
    fasync_fasync(-1, filp, 0);
    printk(KERN_INFO "fasync_release\n");
    return 0;
}

static ssize_t fasync_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct fasync_dev *dev = filp->private_data;
    int ret = 0;
    DECLARE_WAITQUEUE(wait, current);
    mutex_lock(&dev->mutex);
    add_wait_queue(&dev->r_wq, &wait);

    while (dev->len == 0)
    {
        if (filp->f_flags & O_NONBLOCK)
        {
            ret = -EAGAIN;
            goto out_1;
        }
        __set_current_state(TASK_INTERRUPTIBLE);
        mutex_unlock(&dev->mutex);

        schedule();
        if (signal_pending(current))
        {
            ret = -ERESTARTSYS;
            goto out_2;
        }
        mutex_lock(&dev->mutex);
    }

    if (count > dev->len)
        count = dev->len;

    if (copy_to_user(buf, dev->mem, count) != 0)
        ret = -EFAULT;
    else
    {
        dev->len = dev->len - count;
        memcpy(dev->mem, dev->mem + count, dev->len);
        ret = count;
        wake_up_interruptible(&dev->w_wq);
    }

out_1:
    mutex_unlock(&dev->mutex);
out_2:
    remove_wait_queue(&dev->r_wq, &wait);
    set_current_state(TASK_RUNNING);
    return ret;
}

static ssize_t fasync_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    int ret = -ENOMEM;
    struct fasync_dev *dev = filp->private_data;
    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&dev->mutex);
    add_wait_queue(&dev->w_wq, &wait);

    while (dev->len == FASYNC_SIZE)
    {
        if (filp->f_flags & O_NONBLOCK)
        {
            ret = -EAGAIN;
            goto out_1;
        }

        __set_current_state(TASK_INTERRUPTIBLE);

        mutex_unlock(&dev->mutex);

        schedule();
        if (signal_pending(current))
        {
            ret = -ERESTARTSYS;
            goto out_2;
        }
        mutex_lock(&dev->mutex);
    }
    if (count > FASYNC_SIZE - dev->len)
        count = FASYNC_SIZE - dev->len;

    if (copy_from_user(dev->mem + dev->len, buf, count))
        ret = -EFAULT;
    else
    {
        dev->len += count;
        wake_up_interruptible(&dev->r_wq);
        ret = count;

        // 产生异步读信号
        if (dev->fasync_queue)
        {
            kill_fasync(&dev->fasync_queue, SIGIO, POLL_IN);
            printk(KERN_DEBUG "%s kill SIGIO\n", __func__);
        }
    }
out_1:
    mutex_unlock(&dev->mutex);
out_2:
    remove_wait_queue(&dev->w_wq, &wait);
    set_current_state(TASK_RUNNING);
    return ret;
}

static long fasync_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct fasync_dev *dev = filp->private_data;
    int ret = 0;

    // 检查幻数(返回值POSIX标准规定，也用-EINVAL)
    if (_IOC_TYPE(cmd) != FASYNC_MAGIC)
        return -ENOTTY;
    // 检查命令编号
    if (_IOC_NR(cmd) > FASYNC_MAXNR)
        return -ENOTTY;
    // 检查命令方向，并验证用户空间指针的访问权限。
    if (_IOC_DIR(cmd) & _IOC_READ)
        ret = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        ret = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

    if (ret)
        return -EFAULT;

    switch (cmd)
    {
    case FASYNC_CLEAR:
        mutex_lock(&dev->mutex);
        dev->len = 0;
        memset(dev->mem, 0, FASYNC_SIZE);
        mutex_unlock(&dev->mutex);
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static unsigned int fasync_poll(struct file *filp, poll_table *wait)
{
    unsigned int mask = 0;
    struct fasync_dev *dev = filp->private_data;

    mutex_lock(&dev->mutex);
    poll_wait(filp, &dev->r_wq, wait);
    poll_wait(filp, &dev->w_wq, wait);

    if (dev->len != 0)
        mask |= POLLIN | POLLRDNORM;
    if (dev->len != FASYNC_SIZE)
        mask |= POLLOUT | POLLWRNORM;

    mutex_unlock(&dev->mutex);
    return mask;
}

static const struct file_operations fasync_fops =
    {
        .owner = THIS_MODULE,
        .read = fasync_read,
        .write = fasync_write,
        .unlocked_ioctl = fasync_ioctl,
        .poll = fasync_poll,
        .open = fasync_open,
        .release = fasync_release,
        .fasync = fasync_fasync,
};

static int __init fasync_init(void)
{
    int ret = 0, i;
    dev_t devno = MKDEV(fasync_major, fasync_minor);

    if (fasync_major)
        ret = register_chrdev_region(devno, fasync_nr_devs, "fasync");
    else
    {
        ret = alloc_chrdev_region(&devno, fasync_minor, fasync_nr_devs, "fasync");
        fasync_major = MAJOR(devno);
    }

    if (ret < 0)
        return ret;

    fasync_devp = kzalloc(sizeof(struct fasync_dev) * fasync_nr_devs, GFP_KERNEL);
    if (!fasync_devp)
    {
        printk(KERN_WARNING "alloc mem failed");
        ret = -ENOMEM;
        goto out_1;
    }

    fasync_cls = class_create(THIS_MODULE, "fasync");
    if (IS_ERR(fasync_cls))
    {
        printk(KERN_WARNING "Error creating class for ioctl");
        goto out_2;
    }

    for (i = 0; i < fasync_nr_devs; i++)
    {
        cdev_init(&fasync_devp[i].cdev, &fasync_fops);
        fasync_devp[i].cdev.owner = THIS_MODULE;
        ret = cdev_add(&fasync_devp[i].cdev, MKDEV(fasync_major, fasync_minor + i), 1);
        if (ret)
            printk(KERN_WARNING "Error adding cdev for device %d", i);
        else
        {
            fasync_devp[i].class_dev = device_create(fasync_cls, NULL, MKDEV(fasync_major, fasync_minor + i), NULL, "fasync%d", i);
            if (IS_ERR(fasync_devp[i].class_dev))
                printk(KERN_WARNING "Error creating device for device %d", i);
        }
        mutex_init(&fasync_devp[i].mutex);
        init_waitqueue_head(&fasync_devp[i].r_wq);
        init_waitqueue_head(&fasync_devp[i].w_wq);
    }
    return 0;
out_2:
    kfree(fasync_devp);
out_1:
    unregister_chrdev_region(devno, fasync_nr_devs);
    return ret;
}

static void __exit fasync_exit(void)
{
    int i;
    for (i = 0; i < fasync_nr_devs; i++)
    {
        device_destroy(fasync_cls, MKDEV(fasync_major, fasync_minor + i));
        cdev_del(&fasync_devp[i].cdev);
    }
    class_destroy(fasync_cls);
    kfree(fasync_devp);
    unregister_chrdev_region(MKDEV(fasync_major, fasync_minor), fasync_nr_devs);
    printk(KERN_INFO "fasync exit\n");
}

module_param(fasync_major, int, S_IRUGO);
module_param(fasync_minor, int, S_IRUGO);
module_param(fasync_nr_devs, int, S_IRUGO);

module_init(fasync_init);
module_exit(fasync_exit);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");