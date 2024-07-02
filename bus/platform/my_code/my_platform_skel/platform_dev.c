/*
 * @Date: 2024-05-28 14:38:07
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-29 16:05:33
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

struct resource skel_resource[] =
    {
        [0] = {
            .start = 0x139d0000,
            .end = 0x139d0000 + 0x3,
            .flags = IORESOURCE_MEM,
        },
        [1] = {
            .start = 123,
            .end = 123,
            .flags = IORESOURCE_IRQ,
        },
        [2] = {
            .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE | IORESOURCE_IRQ_HIGHEDGE,
        },
};

void skel_release(struct device *dev)
{
    printk("skel_release\n");
}

// 私有数据
unsigned int skel_info[2] = {0x12345678, 0x87654321};

struct platform_device skel_dev =
    {
        .name = "platform_sekl",
        .id = 1,
        .dev =
            {
                .release = skel_release,
                .platform_data = skel_info,
            },
        .resource = skel_resource,
        .num_resources = ARRAY_SIZE(skel_resource), // 资源个数
};

static int __init skel_dev_init(void)
{
    platform_device_register(&skel_dev);
    return 0;
}

static void __exit skel_dev_exit(void)
{
    platform_device_unregister(&skel_dev);
}

module_init(skel_dev_init);
module_exit(skel_dev_exit);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");