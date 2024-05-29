/*
 * @Date: 2024-04-29 11:23:01
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-27 13:17:11
 */
#include <linux/init.h>
#include <linux/module.h>

static int __init hello_init_module(void)
{
	printk(KERN_INFO "Hello World enter\n");
	// pr_info("Hello World enter\n");
	// pr_info 是一个宏，与printk的KERN_INFO等价
	return 0;
}
static void __exit hello_exit_module(void)
{
	printk(KERN_INFO "Hello World exit\n");
}

module_init(hello_init_module);
module_exit(hello_exit_module);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");
