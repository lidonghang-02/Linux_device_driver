/*
 * @Date: 2024-05-17 19:16:18
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-19 20:13:58
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/sched.h>

#include <linux/timer.h>

#define TIMER_MAJOR 0
#define TIMER_MINOR 0
#define TIMER_NR_DEVS 1

static int timer_major = TIMER_MAJOR;
static int timer_minor = TIMER_MINOR;
static int timer_nr_devs = TIMER_NR_DEVS;

static struct class *class = NULL;
struct timer_dev
{
  struct cdev cdev;
  struct device *cls_dev;
  char *buf;
  int delay;
  int loops;
  unsigned long pre_jiffies;
  wait_queue_head_t wq;
  struct timer_list timer;
};

static struct timer_dev *timer_devp;

// 定时器回调函数
void timer_handler(unsigned long data)
{
  struct timer_dev *dev = (struct timer_dev *)data;
  dev->buf += sprintf(dev->buf,
                      "%9li  %3li     %i    %6i   %i   %s\n",
                      jiffies, jiffies - dev->pre_jiffies,
                      in_interrupt() ? 1 : 0, current->pid,
                      smp_processor_id(), current->comm);

  dev->loops--;
  if (dev->loops > 0)
  {
    mod_timer(&dev->timer, jiffies + dev->delay);
    dev->pre_jiffies = jiffies;
  }
  else
  {
    del_timer(&dev->timer);
    wake_up_interruptible(&dev->wq);
  }
}

static int timer_open_func(struct inode *inode, struct file *filp)
{
  struct timer_dev *dev = container_of(inode->i_cdev, struct timer_dev, cdev);
  filp->private_data = dev;
  printk(KERN_INFO "timer_open\n");
  return 0;
}

static int timer_release_func(struct inode *inode, struct file *filp)
{
  printk(KERN_INFO "timer_release\n");
  return 0;
}

static ssize_t timer_read_func(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  ssize_t ret = 0;
  size_t cnt;
  char *tmp_buf;

  struct timer_dev *dev = filp->private_data;

  // 确保只读一次
  if (*f_pos > 0)
    return 0;

  tmp_buf = kmalloc(400, GFP_KERNEL);
  if (!tmp_buf)
    return -ENOMEM;

  dev->loops = 4;
  dev->delay = HZ;
  dev->buf = tmp_buf;
  dev->buf += sprintf(dev->buf, "   time   delta  inirq    pid   cpu command\n");
  dev->buf += sprintf(dev->buf,
                      "%9li  %3li     %i    %6i   %i   %s\n",
                      jiffies, 0L, in_interrupt() ? 1 : 0,
                      current->pid, smp_processor_id(), current->comm);

  // setup_timer在新版本的内核中已经被替换为timer_setup
  setup_timer(&dev->timer, timer_handler, (unsigned long)dev);
  mod_timer(&dev->timer, jiffies + dev->delay);

  // 也可以使用下面方法添加定时器
  // init_timer(&dev->timer);
  // dev->timer.function = timer_handler;
  // dev->timer.data = (unsigned long)dev;
  // dev->timer.expires = jiffies + dev->delay;
  // add_timer(&dev->timer);

  dev->pre_jiffies = jiffies;
  wait_event_interruptible(dev->wq, !dev->loops);
  cnt = dev->buf - tmp_buf;

  if (copy_to_user(buf, tmp_buf, cnt))
  {
    ret = -EFAULT;
    goto out;
  }

  *f_pos += cnt;
  ret = cnt;

out:
  kfree(tmp_buf);
  return ret;
}

static const struct file_operations timer_fops = {
    .owner = THIS_MODULE,
    .open = timer_open_func,
    .release = timer_release_func,
    .read = timer_read_func,
};

static int __init timer_init_module(void)
{
  int ret, i;
  dev_t devno = MKDEV(timer_major, timer_minor);

  if (timer_major)
    ret = register_chrdev_region(devno, timer_nr_devs, "timer");
  else
  {
    ret = alloc_chrdev_region(&devno, timer_minor, timer_nr_devs, "timer");
    timer_major = MAJOR(devno);
  }

  if (ret < 0)
  {
    printk(KERN_ALERT "register_chrdev_region failed with %d\n", ret);
    goto out_1;
  }

  timer_devp = kzalloc(sizeof(struct timer_dev) * timer_nr_devs, GFP_KERNEL);
  if (timer_devp == NULL)
  {
    ret = -ENOMEM;
    printk(KERN_ALERT "Out of memory\n");
    goto out_2;
  }

  class = class_create(THIS_MODULE, "timer_class");
  if (IS_ERR(class))
  {
    ret = PTR_ERR(class);
    printk(KERN_ALERT "class_create failed with %d\n", ret);
    goto out_3;
  }

  for (i = 0; i < timer_nr_devs; i++)
  {
    cdev_init(&timer_devp[i].cdev, &timer_fops);
    timer_devp[i].cdev.owner = THIS_MODULE;
    ret = cdev_add(&timer_devp[i].cdev, MKDEV(timer_major, timer_minor + i), 1);
    if (ret < 0)
      printk(KERN_ALERT "cdev_add failed with %d\n", ret);
    else
    {
      timer_devp[i].cls_dev = device_create(class, NULL, MKDEV(timer_major, timer_minor + i), NULL, "timer%d", i);
      if (IS_ERR(timer_devp[i].cls_dev))
        printk(KERN_ALERT "device_create failed with %d\n", ret);
    }
    init_waitqueue_head(&timer_devp[i].wq);
  }

  printk(KERN_INFO "timer_init\n");
  return 0;

out_3:
  kfree(timer_devp);
out_2:
  unregister_chrdev_region(devno, timer_nr_devs);
out_1:
  return ret;
}

static void __exit timer_exit_module(void)
{
  int i;

  for (i = 0; i < timer_nr_devs; i++)
  {
    device_destroy(class, MKDEV(timer_major, timer_minor + i));
    cdev_del(&timer_devp[i].cdev);
  }
  class_destroy(class);
  kfree(timer_devp);
  unregister_chrdev_region(MKDEV(timer_major, timer_minor), timer_nr_devs);
  printk(KERN_INFO "timer_exit\n");
}

module_param(timer_major, int, S_IRUGO);
module_param(timer_minor, int, S_IRUGO);
module_param(timer_nr_devs, int, S_IRUGO);

module_init(timer_init_module);
module_exit(timer_exit_module);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");
