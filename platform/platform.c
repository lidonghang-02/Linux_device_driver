#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>

// miscdevice结构体在Linux内核中用于定义杂项设备（也称为混合设备或misc设备）的属性。
// 杂项设备是字符设备的一种，当板子上的某个设备没有办法明确分类时，
// 可以使用杂项设备驱动。杂项设备可以自动生成设备节点，
// 并且所有的MISC设备驱动的主设备号都为10，不同的设备使用不同的从设备号。
struct miscdevice misc_device;

static int platform_device_open_func(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "platform_open_func\n");
    return 0;
}
static int platform_device_release_func(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "platform_release_func\n");
    return 0;
}
static const struct file_operations platform_device_fops = {
    .owner = THIS_MODULE,
    .open = platform_device_open_func,
    .release = platform_device_release_func,
};

static int platform_driver_probe_func(struct platform_device *pdev)
{
    int ret;
    // 创建设备节点
    misc_device.minor = MISC_DYNAMIC_MINOR;
    misc_device.name = "platform_device";
    misc_device.fops = &platform_device_fops;

    ret = misc_register(&misc_device);
    if (ret < 0)
    {
        printk(KERN_ERR "Failed to register misc device\n");
        return ret;
    }

    // dev_info 是一个宏，通常用于在内核驱动中打印信息。
    dev_info(&pdev->dev, "Platform device probed\n");

    printk(KERN_INFO "Platform device probed\n");
    return 0;
}

static int platform_driver_remove_func(struct platform_device *pdev)
{
    printk(KERN_INFO "Platform device removed\n");
    misc_deregister(&misc_device);
    dev_info(&pdev->dev, "Platform device removed\n");
    return 0;
}

static struct platform_driver platform_driver = {
    .driver = {
        .name = "platform_device",
        .owner = THIS_MODULE,
    },
    .probe = platform_driver_probe_func,
    .remove = platform_driver_remove_func,
};

module_platform_driver(platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lidonghang-02");