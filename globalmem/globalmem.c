#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "beep.h"

// mem大小，4k
#define GLOBALMEM_SIZE 0x1000
// 设备数量，次设备号为0-9
#define DEVICE_NUM 10

static int major = 230;
module_param(major, int, S_IRUGO);

// 设备类，在insmod设备后可以在/sys/class/下看到
struct class *class = NULL;
struct globalmem_dev
{
    struct cdev cdev;
    struct device *class_dev;
    struct mutex mutex;
    unsigned char mem[GLOBALMEM_SIZE];
};
struct globalmem_dev *globalmem_devp;

static int globalmem_open(struct inode *inode, struct file *filp)
{
    // container_of 是一个宏，用于根据一个包含某成员的结构体中的该成员的地址来获取整个结构体的地址。
    // 返回整个结构体的指针
    struct globalmem_dev *dev = container_of(inode->i_cdev, struct globalmem_dev, cdev);

    filp->private_data = dev;
    return 0;
}
static int globalmem_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static long globalmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct globalmem_dev *dev = filp->private_data;
    int ret;

    // 检查命令类型
    if (_IOC_TYPE(cmd) != DEV_FIFO_TYPE)
    {
        pr_err("cmd   %u,bad magic 0x%x/0x%x.\n", cmd, _IOC_TYPE(cmd), DEV_FIFO_TYPE);
        return -ENOTTY;
    }
    // 检查命令方向，并验证用户空间指针的访问权限。
    if (_IOC_DIR(cmd) & _IOC_READ)
        ret = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        ret = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

    if (ret)
    {
        pr_err("bad   access %ld.\n", ret);
        return -EFAULT;
    }

    switch (cmd)
    {
    case DEV_FIFO_CLEAN:
        memset(dev->mem, 0, GLOBALMEM_SIZE);
        printk(KERN_INFO "globalmem clear\n");
        break;

    default:
        return -EINVAL;
    }
    return 0;
}

static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned long count = size;
    int ret = 0;
    struct globalmem_dev *dev = filp->private_data;

    if (p >= GLOBALMEM_SIZE)
        return 0;
    if (count > GLOBALMEM_SIZE - p)
        count = GLOBALMEM_SIZE - p;

    // 上锁
    mutex_lock(&dev->mutex);
    if (copy_to_user(buf, dev->mem + p, count))
        ret = -EFAULT;
    else
    {
        *ppos += count;
        ret = count;
        printk(KERN_INFO "read %u bytes(s) from %lu\n", count, p);
    }
    // 解锁
    mutex_unlock(&dev->mutex);

    return ret;
}

static ssize_t globalmem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned long count = size;
    int ret = 0;
    struct globalmem_dev *dev = filp->private_data;

    if (p >= GLOBALMEM_SIZE)
        return -ENOMEM;
    if (count > GLOBALMEM_SIZE - p)
        count = GLOBALMEM_SIZE - p;

    mutex_lock(&dev->mutex);
    if (copy_from_user(dev->mem + p, buf, count))
        ret = -EFAULT;
    else
    {
        *ppos += count;
        ret = count;
        printk(KERN_INFO "write %u bytes(s) to %lu\n", count, p);
    }
    mutex_unlock(&dev->mutex);

    return ret;
}

// 更改打开文件的文件偏移量
// SEEK_SET：文件的开始处--0
// SEEK_CUR：文件的当前位置--1
// SEEK_END：文件的结束处--2

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
{
    loff_t ret = 0;
    switch (orig)
    {
    case 0:
        if (offset < 0)
        {
            ret = -EINVAL;
            break;
        }
        if ((unsigned int)offset > GLOBALMEM_SIZE)
        {
            ret = -EINVAL;
            break;
        }
        filp->f_pos = (unsigned int)offset;
        ret = filp->f_pos;
        break;
    case 1:
        if ((filp->f_pos + offset) > GLOBALMEM_SIZE)
        {
            ret = -EINVAL;
            break;
        }
        if ((filp->f_pos + offset) < 0)
        {
            ret = -EINVAL;
            break;
        }
        filp->f_pos += offset;
        ret = filp->f_pos;
        break;
    case 2:
        if (offset > 0)
        {
            ret = -EINVAL;
            break;
        }
        if ((GLOBALMEM_SIZE + offset) < 0)
        {
            ret = -EINVAL;
            break;
        }
        filp->f_pos = GLOBALMEM_SIZE + offset;
        ret = filp->f_pos;
        break;

    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static const struct file_operations globalmem_fops = {
    .owner = THIS_MODULE,
    .open = globalmem_open,
    .release = globalmem_release,
    .unlocked_ioctl = globalmem_ioctl,
    .read = globalmem_read,
    .write = globalmem_write,
    .llseek = globalmem_llseek,
};
static void globalmem_setup(struct globalmem_dev *dev, int index)
{
    // 获取设备号
    int err, devno = MKDEV(major, index);

    cdev_init(&dev->cdev, &globalmem_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_NOTICE "Error %d adding globalmem%d", err, index);
    else
    {

        dev->class_dev = NULL;
        dev->class_dev = device_create(class, NULL, devno, NULL, "globalmem%d", index);
        if (IS_ERR(dev->class_dev))
        {
            printk(KERN_NOTICE "Error creating device for globalmem%d", index);
        }
        // 初始化互斥锁
        mutex_init(&dev->mutex);
    }
}
static int __init globalmem_init(void)
{
    int ret = 0;
    dev_t devno = MKDEV(major, 0);
    int i;

    // 注册设备号
    if (major)
        ret = register_chrdev_region(devno, DEVICE_NUM, "globalmem");
    else
    {
        ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "globalmem");
        major = MAJOR(devno);
    }

    if (ret < 0)
        return ret;

    // 分配DEVICE_NUM个内存
    globalmem_devp = kzalloc(sizeof(struct globalmem_dev) * DEVICE_NUM, GFP_KERNEL);
    if (!globalmem_devp)
    {
        ret = -ENOMEM;
        goto out_err_1;
    }
    // 创建一个类
    class = class_create(THIS_MODULE, "globalmem");
    if (IS_ERR(class))
    {
        printk(KERN_NOTICE "Error creating class for globalmem");
        goto out_err_2;
    }

    // 初始化globalmem_devp数组
    // globalmem_devp + i = globalmem_devp + i * sizeof(struct globalmem_dev)
    // C语言中，指针算术是自动根据指针指向的数据类型来调整的，不需要显示指出
    for (i = 0; i < DEVICE_NUM; i++)
        globalmem_setup(globalmem_devp + i, i);

    printk("globalmem module init\n");
    return 0;
out_err_2:
    kfree(globalmem_devp);
out_err_1:
    unregister_chrdev_region(devno, DEVICE_NUM);
    return ret;
}
module_init(globalmem_init);

static void __exit globalmem_exit(void)
{
    int i;
    for (i = 0; i < DEVICE_NUM; i++)
    {
        device_destroy(class, MKDEV(major, i));
        cdev_del(&globalmem_devp[i].cdev);
    }
    class_destroy(class);

    kfree(globalmem_devp);
    unregister_chrdev_region(MKDEV(major, 0), DEVICE_NUM);
    printk(KERN_INFO "globalmem module exit\n");
}
module_exit(globalmem_exit);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");