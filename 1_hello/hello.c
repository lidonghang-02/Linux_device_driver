#include <linux/init.h>
#include <linux/module.h>

static int __init hello_init(void)
{
  printk(KERN_INFO "Hello World enter\n");
  return 0;
}
static void __exit hello_exit(void)
{
  printk(KERN_INFO "Hello World exit\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hello World Module");
