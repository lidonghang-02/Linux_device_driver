#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>

#define CREATE_CHR_MAJOR 0
#define CREATE_CHR_NR_DEVS 2

static int create_chr_major = CREATE_CHR_MAJOR;
static int create_chr_minor = 0;
static int create_chr_nr_devs = CREATE_CHR_NR_DEVS;

module_param(create_chr_major, int, S_IRUGO);
module_param(create_chr_minor, int, S_IRUGO);
module_param(create_chr_nr_devs, int, S_IRUGO);

struct class *cls = NULL;
struct create_char_dev
{
    struct cdev cdev;
    struct device *class_dev;
};

struct create_char_dev *create_chr_devp;

static int create_chr_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "open create_dev%d %d\n", iminor(inode), MINOR(inode->i_cdev->dev));
    return 0;
}
static int create_chr_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "release create_dev\n");
    return 0;
}
static ssize_t create_chr_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    printk(KERN_INFO "read create_dev\n");
    return 0;
}
static ssize_t create_chr_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    printk(KERN_INFO "write create_dev\n");
    return size; // 不能返回0，否则会不停的写
}

static struct file_operations create_chr_fops = {
    // 字符设备的操作函数
    .owner = THIS_MODULE,
    .open = create_chr_open,
    .release = create_chr_release,
    .read = create_chr_read,
    .write = create_chr_write,
};

static int __init create_init(void)
{
    int ret, i;
    dev_t devno = MKDEV(create_chr_major, create_chr_minor);
    if (create_chr_major)
        ret = register_chrdev_region(devno, create_chr_nr_devs, "create_chr"); // 使用指定的设备号分配
    else
    {
        ret = alloc_chrdev_region(&devno, create_chr_minor, create_chr_nr_devs, "create_chr"); // 动态分配主设备号
        create_chr_major = MAJOR(devno);
    }
    if (ret < 0)
    {
        printk(KERN_WARNING "create: can't get major %d\n", create_chr_major);
        goto out_err_1;
    }

    create_chr_devp = kzalloc(sizeof(struct create_char_dev) * create_chr_nr_devs, GFP_KERNEL); // 给字符设备分配空间，这里create_chr_nr_devs为2
    if (!create_chr_devp)
    {
        printk(KERN_WARNING "alloc mem failed");
        ret = -ENOMEM;
        goto out_err_2;
    }

    cls = class_create(THIS_MODULE, "create_chr"); // 创建一个类
    if (IS_ERR(cls))
    {
        ret = PTR_ERR(cls);
        printk(KERN_WARNING "create_chr: can't create class\n");
        goto out_err_2;
    }

    for (i = 0; i < create_chr_nr_devs; i++)
    {
        // 初始化字符设备结构
        cdev_init(&create_chr_devp[i].cdev, &create_chr_fops);
        create_chr_devp[i].cdev.owner = THIS_MODULE;
        ret = cdev_add(&create_chr_devp[i].cdev, MKDEV(create_chr_major, create_chr_minor + i), 1); // 添加该字符设备到系统中
        if (ret)
        {
            printk(KERN_WARNING "fail add create_dev%d", i);
            continue;
        }
        create_chr_devp[i].class_dev = device_create(cls, NULL, devno + i, NULL, "create_dev%d", i); // 创建设备节点
        if (IS_ERR(create_chr_devp[i].class_dev))
            printk(KERN_WARNING "fail create create_dev%d", i);
    }

    printk(KERN_INFO "create_chr init\n");
    return 0;

out_err_2:
    unregister_chrdev_region(devno, create_chr_nr_devs);
out_err_1:
    return ret; // 返回错误，模块无法正常加载
}
module_init(create_init);

static void __exit create_exit(void)
{
    int i;
    for (i = 0; i < create_chr_nr_devs; i++)
    {
        device_destroy(cls, MKDEV(create_chr_major, create_chr_minor + i));
        cdev_del(&create_chr_devp[i].cdev);
    }
    class_destroy(cls);
    kfree(create_chr_devp);
    unregister_chrdev_region(MKDEV(create_chr_major, create_chr_minor), create_chr_nr_devs); // 移除模块时释放设备号
    printk(KERN_INFO "create_chr exit\n");
}
module_exit(create_exit);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");
