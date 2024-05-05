#include <linux/init.h>
#include <linux/module.h>

static int param_value = 0;
static char *param_name = "default";

// 在模块加载时，可以通过“insmod 模块名 参数名=参数值”来设置参数
module_param(param_value, int, S_IRUGO);
module_param(param_name, charp, S_IRUGO);

static int __init param_init(void)
{
    printk(KERN_INFO "param module init\n");
    printk(KERN_INFO "param_value = %d\n", param_value);
    printk(KERN_INFO "param_name = %s\n", param_name);
    return 0;
}

static void __exit param_exit(void)
{
    printk(KERN_INFO "param module exit\n");
}

module_init(param_init);
module_exit(param_exit);

MODULE_LICENSE("GPL");
