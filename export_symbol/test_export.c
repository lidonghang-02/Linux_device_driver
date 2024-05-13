#include <linux/module.h>
#include <linux/kernel.h>

extern int exported_function(int a, int b); // 声明导出的函数

static int __init test_export_init(void)
{
    int result = exported_function(2, 3); // 调用导出的函数
    printk(KERN_INFO "test_export: result = %d\n", result);
    return 0;
}

static void __exit test_export_exit(void)
{
    printk(KERN_INFO "test_export unloaded\n");
}

module_init(test_export_init);
module_exit(test_export_exit);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");