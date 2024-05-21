#include <linux/module.h>
#include <linux/platform_device.h>

static struct platform_device* pdev;

static int __init platform_init_module(void)
{
  int ret = 0;

  // platform_device_alloc 函数
  // 用于分配一个新的 platform_device 结构体，并初始化其中的一些字段。
  // 这个函数不会将设备添加到系统中，只是简单地分配并初始化结构体。
  pdev = platform_device_alloc("my_platform_device", -1);
  if (!pdev)
    return -ENOMEM;

  // platform_device_add 函数
  // 用于将之前通过 platform_device_alloc 分配并初始化
  // 的 platform_device 结构体添加到系统中。
  // 这个函数会触发与设备相关的驱动程序进行匹配，
  // 并可能触发设备的探测和初始化过程。
  ret = platform_device_add(pdev);
  if (ret)
  {
    // platform_device_put 函数用于释放通过 platform_device_alloc 分配的平台设备。
    // 这个函数会递减设备的引用计数，并在引用计数达到 0 时释放设备占用的内存。
    // 如果设备已经被添加到系统中（通过 platform_device_add），
    // 那么你需要先使用 platform_device_unregister 来注销设备，
    // 然后才能使用 platform_device_put 来释放它
    platform_device_put(pdev);
    return ret;
  }
  printk(KERN_INFO "platform_init_module \n");
  return 0;
}
static void __exit platform_exit_module(void)
{
  printk(KERN_INFO "platform_exit_module \n");
  platform_device_unregister(pdev);
  platform_device_put(pdev);
}

module_init(platform_init_module);
module_exit(platform_exit_module);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");
