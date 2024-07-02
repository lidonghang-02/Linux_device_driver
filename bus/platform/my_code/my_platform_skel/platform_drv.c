/*
 * @Date: 2024-05-28 14:49:08
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-07-01 15:24:35
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#define SKEL_MAJOR 230

static int platform_skel_major = SKEL_MAJOR;

static struct class *platform_skel_class;
struct my_platform_data
{
    int my_data;
    unsigned int __iomem *reg1;
    unsigned int __iomem *reg2;

    struct cdev cdev;
};

static int platform_skel_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "platform_skel_open\n");
    return 0;
}

static int platform_skel_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "platform_skel_release\n");
    return 0;
}

static struct file_operations platform_skel_fops =
    {
        .owner = THIS_MODULE,
        .open = platform_skel_open,
        .release = platform_skel_release,
};

static int platform_skel_probe(struct platform_device *pdev)
{
    struct my_platform_data *pdata;
    dev_t devno;
    unsigned int *skel_info;
    int ret = 0;
    struct resource *res0;
    struct resource *res1;

    pdata = devm_kzalloc(&pdev->dev, sizeof(struct my_platform_data), GFP_KERNEL);
    if (!pdata)
        return -ENOMEM;
    // 获取私有数据
    skel_info = dev_get_platdata(&pdev->dev);

    pdata->my_data = *skel_info;

    res0 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    res1 = platform_get_resource(pdev, IORESOURCE_IRQ, 1);

    // 转化为虚拟地址
    // res0->end - res0->start + 1 = resource_size(res0)
    pdata->reg1 = devm_ioremap(&pdev->dev, res0->start, resource_size(res0));
    pdata->reg2 = devm_ioremap(&pdev->dev, res1->start, res1->end - res1->start + 1);

    // 注册字符设备
    devno = MKDEV(platform_skel_major, pdev->id);

    ret = register_chrdev_region(devno, 1, "platform_skel");
    if (ret)
        return ret;

    cdev_init(&pdata->cdev, &platform_skel_fops);
    ret = cdev_add(&pdata->cdev, devno, 1);
    if (ret)
    {
        unregister_chrdev_region(devno, 1);
        return ret;
    }
    device_create(platform_skel_class, NULL, devno, NULL, "platform_skel%d", pdev->id);

    platform_set_drvdata(pdev, pdata);

    return 0;
}

static int platform_skel_remove(struct platform_device *pdev)
{
    struct my_platform_data *pdata = platform_get_drvdata(pdev);

    cdev_del(&pdata->cdev);
    device_destroy(platform_skel_class, MKDEV(platform_skel_major, pdev->id));
    unregister_chrdev_region(MKDEV(platform_skel_major, pdev->id), 1);
    return 0;
}

static struct platform_device_id platform_skel_ids[] =
    {
        {"platform_skel", 0},
        {},
};
MODULE_DEVICE_TABLE(platform, platform_skel_ids);

static struct platform_driver platform_skel_driver = {
    .probe = platform_skel_probe,
    .remove = platform_skel_remove,
    .driver =
        {
            .name = "platform_skel",
        },
    .id_table = platform_skel_ids,
};

static int __init platform_skel_init(void)
{
    platform_skel_class = class_create(THIS_MODULE, "platform_skel");
    platform_driver_register(&platform_skel_driver);
    return 0;
}

static void __exit platform_skel_exit(void)
{
    platform_driver_unregister(&platform_skel_driver);
    class_destroy(platform_skel_class);
}

module_init(platform_skel_init);
module_exit(platform_skel_exit);

MODULE_AUTHOR("lidoghang-02");
MODULE_LICENSE("GPL");
