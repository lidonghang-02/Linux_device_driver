
#include <linux/module.h>
#include <linux/kernel.h>

int exported_function(int a, int b)
{
	return a + b;
}
EXPORT_SYMBOL(exported_function);

static int __init export_symbol_init(void)
{
	printk(KERN_INFO "export_symbol loaded\n");
	return 0;
}

static void __exit export_symbol_exit(void)
{
	printk(KERN_INFO "export_symbol unloaded\n");
}

module_init(export_symbol_init);
module_exit(export_symbol_exit);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");