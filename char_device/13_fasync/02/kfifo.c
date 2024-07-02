#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/kfifo.h>

#define KFIFO_SIZE 64
#define KFIFO_NAME "kfifo_dev"
#define KFIFO_MAJOR 0

static int kfifo_major = KFIFO_MAJOR;

struct kfifo_dev
{
    struct cdev cdev;
    struct device *dev;

    wait_queue_head_t read_wait;
    wait_queue_head_t write_wait;
    struct kfifo mykfifo;
    struct fasync_struct *fasync;
};
struct class *cls;
static struct kfifo_dev *kfifo_devp;

static int kfifo_fasync_func(int fd, struct file *file, int on)
{
    struct kfifo_dev *dev = file->private_data;
    return fasync_helper(fd, file, on, &dev->fasync);
}

static int kfifo_open_func(struct inode *inode, struct file *file)
{
    struct kfifo_dev *dev = container_of(inode->i_cdev, struct kfifo_dev, cdev);
    file->private_data = dev;
    return 0;
}

static int kfifo_release_func(struct inode *inode, struct file *file)
{
    kfifo_fasync_func(-1, file, 0);
    return 0;
}

static ssize_t kfifo_read_func(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    struct kfifo_dev *dev = file->private_data;
    int ret;
    int actual_read;

    if (kfifo_is_empty(&dev->mykfifo))
    {
        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;

        ret = wait_event_interruptible(dev->read_wait, !kfifo_is_empty(&dev->mykfifo));
        if (ret)
            return ret;
    }

    ret = kfifo_to_user(&dev->mykfifo, buf, count, &actual_read);
    if (ret)
        return ret;

    if (!kfifo_is_full(&dev->mykfifo))
    {
        wake_up_interruptible(&dev->write_wait);
        kill_fasync(&dev->fasync, SIGIO, POLL_OUT);
    }
    printk(KERN_INFO "%s:%s,actual_read=%d,pos=%lld\n", __FILE__, __func__, actual_read, *pos);
    return actual_read;
}

static ssize_t kfifo_write_func(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    struct kfifo_dev *dev = file->private_data;
    int ret;
    int actual_write;

    if (kfifo_is_full(&dev->mykfifo))
    {
        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;

        ret = wait_event_interruptible(dev->write_wait, !kfifo_is_full(&dev->mykfifo));
        if (ret)
            return ret;
    }

    ret = kfifo_from_user(&dev->mykfifo, buf, count, &actual_write);
    if (ret)
        return ret;

    if (!kfifo_is_empty(&dev->mykfifo))
    {
        wake_up_interruptible(&dev->read_wait);
        kill_fasync(&dev->fasync, SIGIO, POLL_IN);
    }
    printk(KERN_INFO "%s:%s,actual_write=%d,pos=%lld\n", __FILE__, __func__, actual_write, *pos);
    return actual_write;
}

static unsigned int kfifo_poll_func(struct file *file, poll_table *wait)
{
    struct kfifo_dev *dev = file->private_data;
    int mask = 0;
    poll_wait(file, &dev->read_wait, wait);
    poll_wait(file, &dev->write_wait, wait);

    if (!kfifo_is_empty(&dev->mykfifo))
        mask |= POLLIN | POLLRDNORM;
    if (!kfifo_is_full(&dev->mykfifo))
        mask |= POLLOUT | POLLWRNORM;

    return mask;
}

static const struct file_operations kfifo_fops = {
    .owner = THIS_MODULE,
    .open = kfifo_open_func,
    .release = kfifo_release_func,
    .read = kfifo_read_func,
    .write = kfifo_write_func,
    .poll = kfifo_poll_func,
    .fasync = kfifo_fasync_func,
};

static int __init kfifo_init_module(void)
{
    int ret;
    dev_t devno;

    ret = alloc_chrdev_region(&devno, 0, 1, KFIFO_NAME);
    if (ret)
    {
        printk(KERN_ALERT "Failed to allocate a major number.\n");
        return ret;
    }
    kfifo_major = MAJOR(devno);

    kfifo_devp = kzalloc(sizeof(struct kfifo_dev), GFP_KERNEL);
    if (!kfifo_devp)
    {
        ret = -ENOMEM;
        goto fail_alloc;
    }

    cdev_init(&kfifo_devp->cdev, &kfifo_fops);
    kfifo_devp->cdev.owner = THIS_MODULE;

    ret = cdev_add(&kfifo_devp->cdev, devno, 1);
    if (ret)
        goto fail_cdev;

    cls = class_create(THIS_MODULE, KFIFO_NAME);
    if (IS_ERR(cls))
    {
        ret = PTR_ERR(cls);
        goto fail_class;
    }

    kfifo_devp->dev = device_create(cls, NULL, devno, NULL, KFIFO_NAME);
    if (IS_ERR(kfifo_devp->dev))
    {
        ret = PTR_ERR(kfifo_devp->dev);
        goto fail_device;
    }

    init_waitqueue_head(&kfifo_devp->read_wait);
    init_waitqueue_head(&kfifo_devp->write_wait);

    ret = kfifo_alloc(&kfifo_devp->mykfifo, KFIFO_SIZE, GFP_KERNEL);
    if (ret)
    {
        ret = -ENOMEM;
        goto fail_kfifo;
    }

    printk(KERN_INFO "successfully loaded the module.\n");

    return 0;

fail_kfifo:
    device_destroy(cls, devno);
fail_device:
    class_destroy(cls);
fail_class:
    cdev_del(&kfifo_devp->cdev);
fail_cdev:
    kfree(kfifo_devp);
fail_alloc:
    unregister_chrdev_region(devno, 1);
    return ret;
}

static void __exit kfifo_exit_module(void)
{
    device_destroy(cls, MKDEV(kfifo_major, 0));
    class_destroy(cls);
    cdev_del(&kfifo_devp->cdev);
    kfree(kfifo_devp);
    unregister_chrdev_region(MKDEV(kfifo_major, 0), 1);
}

module_init(kfifo_init_module);
module_exit(kfifo_exit_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lidonghang-02");