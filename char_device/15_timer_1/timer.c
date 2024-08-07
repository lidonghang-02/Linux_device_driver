/*
 * @Date: 2024-05-04 20:07:19
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-29 16:35:10
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/device.h>

static int major = 320;

struct class *class = NULL;
struct timer_dev
{
  struct cdev cdev;
  struct device *class_dev;
  atomic_t cnt;
  // 定时器
  struct timer_list second_timer;
};
static struct timer_dev *timer_devp;

static void timer_handler(unsigned long data)
{
  struct timer_dev *timer_dev = (struct timer_dev *)data;
  // 原子操作，cnt+1
  atomic_inc(&timer_dev->cnt);
  printk(KERN_INFO "timer_handler: cnt = %d\n", atomic_read(&timer_dev->cnt));
  // 设置定时器的到期时间
  // jiffies表示自系统启动以来经过的“节拍数”（tick counts）
  // HZ表示每秒的节拍数（tick rate），这里HZ=100,即定时器1s后到期
  timer_dev->second_timer.expires = jiffies + HZ;
  // 添加定时器
  add_timer(&timer_dev->second_timer);
}

static int timer_open_func(struct inode *inode, struct file *filp)
{
  struct timer_dev *dev = container_of(inode->i_cdev, struct timer_dev, cdev);
  // 定时器到期后的处理函数
  dev->second_timer.function = &timer_handler;
  // 处理函数的参数
  dev->second_timer.data = (unsigned long)dev;
  // 设置定时器的到期时间
  dev->second_timer.expires = jiffies + HZ;
  // 初始化定时器
  init_timer(&dev->second_timer);
  add_timer(&dev->second_timer);
  // 初始化计数器，原子操作
  atomic_set(&dev->cnt, 0);

  filp->private_data = dev;
  return 0;
}

static int timer_release_func(struct inode *inode, struct file *filp)
{
  struct timer_dev *dev = filp->private_data;
  // 删除定时器
  del_timer(&dev->second_timer);
  return 0;
}

static ssize_t timer_read_func(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  struct timer_dev *dev = filp->private_data;
  // 原子操作
  int cnt = atomic_read(&dev->cnt);
  if (put_user(cnt, (int *)buf))
    return -EFAULT;

  return sizeof(unsigned int);
}

static const struct file_operations timer_fops = {
    .owner = THIS_MODULE,
    .open = timer_open_func,
    .release = timer_release_func,
    .read = timer_read_func,
};

static int __init timer_init_module(void)
{
  int devno = MKDEV(major, 0);
  int ret;

  if (major)
    ret = register_chrdev_region(devno, 1, "timer");
  else
  {
    ret = alloc_chrdev_region(&devno, 0, 1, "timer");
    major = MAJOR(devno);
  }
  if (ret < 0)
    return ret;

  timer_devp = kzalloc(sizeof(struct timer_dev), GFP_KERNEL);
  if (timer_devp == NULL)
  {
    ret = -ENOMEM;
    goto out_err_1;
  }
  class = class_create(THIS_MODULE, "timer");
  if (IS_ERR(class))
  {
    ret = PTR_ERR(class);
    goto out_err_1;
  }

  cdev_init(&timer_devp->cdev, &timer_fops);
  timer_devp->cdev.owner = THIS_MODULE;
  ret = cdev_add(&timer_devp->cdev, devno, 1);
  if (ret)
  {
    printk(KERN_NOTICE "Error %d adding timer%d", ret, 0);
    goto out_err_1;
  }

  timer_devp->class_dev = device_create(class, NULL, devno, NULL, "timer");

  if (IS_ERR(timer_devp->class_dev))
    printk(KERN_NOTICE "Error creating device for timer");

  printk(KERN_INFO "timer module init\n");
  return 0;

out_err_1:
  unregister_chrdev_region(devno, 1);
  return ret;
}

static void __exit timer_exit_module(void)
{
  device_destroy(class, timer_devp->cdev.dev);
  class_destroy(class);
  cdev_del(&timer_devp->cdev);
  kfree(timer_devp);
  unregister_chrdev_region(MKDEV(major, 0), 1);
  printk(KERN_INFO "timer module exit\n");
}

module_param(major, int, S_IRUGO);

module_init(timer_init_module);
module_exit(timer_exit_module);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");
