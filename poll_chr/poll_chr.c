#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/poll.h>

#include "poll_chr.h"

#define POLL_CHR_SIZE 0x10
#define POLL_CHR_MAJOR 0
#define POLL_CHR_MINOR 0
#define POLL_CHR_NR_DEVS 1

static int poll_chr_major = POLL_CHR_MAJOR;
static int poll_chr_minor = POLL_CHR_MINOR;
static int poll_chr_nr_devs = POLL_CHR_NR_DEVS;

static struct class *poll_chr_cls;
struct poll_chr_dev
{
    struct cdev cdev;
    struct device *class_dev;
    unsigned int len;
    unsigned char mem[POLL_CHR_SIZE];

    struct mutex mutex;
    wait_queue_head_t r_wq;
    wait_queue_head_t w_wq;
};

static struct poll_chr_dev *poll_chr_devp;

static int poll_chr_open(struct inode *inode, struct file *filp)
{
    struct poll_chr_dev *dev = container_of(inode->i_cdev, struct poll_chr_dev, cdev);
    filp->private_data = dev;
    printk(KERN_INFO "poll_chr_open\n");
    return 0;
}

static int poll_chr_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "poll_chr_release\n");
    return 0;
}

static ssize_t poll_chr_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct poll_chr_dev *dev = filp->private_data;
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

static ssize_t poll_chr_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    int ret = -ENOMEM;
    struct poll_chr_dev *dev = filp->private_data;
    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&dev->mutex);
    add_wait_queue(&dev->w_wq, &wait);

    while (dev->len == POLL_CHR_SIZE)
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
    if (count > POLL_CHR_SIZE - dev->len)
        count = POLL_CHR_SIZE - dev->len;

    if (copy_from_user(dev->mem + dev->len, buf, count))
        ret = -EFAULT;
    else
    {
        dev->len += count;
        wake_up_interruptible(&dev->r_wq);
        ret = count;
    }
out_1:
    mutex_unlock(&dev->mutex);
out_2:
    remove_wait_queue(&dev->w_wq, &wait);
    set_current_state(TASK_RUNNING);
    return ret;
}

static long poll_chr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct poll_chr_dev *dev = filp->private_data;
    int ret = 0;

    // 检查幻数(返回值POSIX标准规定，也用-EINVAL)
    if (_IOC_TYPE(cmd) != POLL_CHR_MAGIC)
        return -ENOTTY;
    // 检查命令编号
    if (_IOC_NR(cmd) > POLL_CHR_MAXNR)
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
    case POLL_CHR_CLEAR:
        mutex_lock(&dev->mutex);
        dev->len = 0;
        memset(dev->mem, 0, POLL_CHR_SIZE);
        mutex_unlock(&dev->mutex);
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static unsigned int poll_chr_poll(struct file *filp, poll_table *wait)
{
    unsigned int mask = 0;
    struct poll_chr_dev *dev = filp->private_data;

    mutex_lock(&dev->mutex);
    poll_wait(filp, &dev->r_wq, wait);
    poll_wait(filp, &dev->w_wq, wait);

    if (dev->len != 0)
        mask |= POLLIN | POLLRDNORM;
    if (dev->len != POLL_CHR_SIZE)
        mask |= POLLOUT | POLLWRNORM;

    mutex_unlock(&dev->mutex);
    return mask;
}

static const struct file_operations poll_chr_fops =
    {
        .owner = THIS_MODULE,
        .read = poll_chr_read,
        .write = poll_chr_write,
        .unlocked_ioctl = poll_chr_ioctl,
        .poll = poll_chr_poll,
        .open = poll_chr_open,
        .release = poll_chr_release,
};

static int __init poll_chr_init(void)
{
    int ret = 0, i;
    dev_t devno = MKDEV(poll_chr_major, poll_chr_minor);

    if (poll_chr_major)
        ret = register_chrdev_region(devno, poll_chr_nr_devs, "poll_chr");
    else
    {
        ret = alloc_chrdev_region(&devno, poll_chr_minor, poll_chr_nr_devs, "poll_chr");
        poll_chr_major = MAJOR(devno);
    }

    if (ret < 0)
        return ret;

    poll_chr_devp = kzalloc(sizeof(struct poll_chr_dev) * poll_chr_nr_devs, GFP_KERNEL);
    if (!poll_chr_devp)
    {
        printk(KERN_WARNING "alloc mem failed");
        ret = -ENOMEM;
        goto out_1;
    }

    poll_chr_cls = class_create(THIS_MODULE, "poll_chr");
    if (IS_ERR(poll_chr_cls))
    {
        printk(KERN_WARNING "Error creating class for ioctl");
        goto out_2;
    }

    for (i = 0; i < poll_chr_nr_devs; i++)
    {
        cdev_init(&poll_chr_devp[i].cdev, &poll_chr_fops);
        poll_chr_devp[i].cdev.owner = THIS_MODULE;
        ret = cdev_add(&poll_chr_devp[i].cdev, MKDEV(poll_chr_major, poll_chr_minor + i), 1);
        if (ret)
            printk(KERN_WARNING "Error adding cdev for device %d", i);
        else
        {
            poll_chr_devp[i].class_dev = device_create(poll_chr_cls, NULL, MKDEV(poll_chr_major, poll_chr_minor + i), NULL, "poll_chr%d", i);
            if (IS_ERR(poll_chr_devp[i].class_dev))
                printk(KERN_WARNING "Error creating device for device %d", i);
        }
        mutex_init(&poll_chr_devp[i].mutex);
        init_waitqueue_head(&poll_chr_devp[i].r_wq);
        init_waitqueue_head(&poll_chr_devp[i].w_wq);
    }
    return 0;
out_2:
    kfree(poll_chr_devp);
out_1:
    unregister_chrdev_region(devno, poll_chr_nr_devs);
    return ret;
}

static void __exit poll_chr_exit(void)
{
    int i;
    for (i = 0; i < poll_chr_nr_devs; i++)
    {
        device_destroy(poll_chr_cls, MKDEV(poll_chr_major, poll_chr_minor + i));
        cdev_del(&poll_chr_devp[i].cdev);
    }
    class_destroy(poll_chr_cls);
    kfree(poll_chr_devp);
    unregister_chrdev_region(MKDEV(poll_chr_major, poll_chr_minor), poll_chr_nr_devs);
    printk(KERN_INFO "poll_chr exit\n");
}

module_param(poll_chr_major, int, S_IRUGO);
module_param(poll_chr_minor, int, S_IRUGO);
module_param(poll_chr_nr_devs, int, S_IRUGO);

module_init(poll_chr_init);
module_exit(poll_chr_exit);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");