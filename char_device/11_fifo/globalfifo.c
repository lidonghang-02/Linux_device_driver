/*
 * @Date: 2024-05-12 10:19:14
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-26 20:01:32
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/sched.h>

#include "globalfifo.h"

#define GLOBALFIFO_SIZE 0x10
#define GLOBALFIFO_MAJOR 0
#define GLOBALFIFO_MINOR 0
#define GLOBALFIFO_NR_DEVS 1

static int globalfifo_major = GLOBALFIFO_MAJOR;
static int globalfifo_minor = GLOBALFIFO_MINOR;
static int globalfifo_nr_devs = GLOBALFIFO_NR_DEVS;

static struct class *globalfifo_cls;
struct globalfifo_dev
{
  struct cdev cdev;
  struct device *class_dev;
  unsigned int len;
  unsigned char mem[GLOBALFIFO_SIZE];

  struct mutex mutex;
  wait_queue_head_t r_wq;
  wait_queue_head_t w_wq;
};

static struct globalfifo_dev *globalfifo_devp;

static int globalfifo_open_func(struct inode *inode, struct file *filp)
{
  struct globalfifo_dev *dev = container_of(inode->i_cdev, struct globalfifo_dev, cdev);
  filp->private_data = dev;
  printk(KERN_INFO "globalfifo_open\n");
  return 0;
}

static int globalfifo_release_func(struct inode *inode, struct file *filp)
{
  printk(KERN_INFO "globalfifo_release\n");
  return 0;
}

static ssize_t globalfifo_read_func(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  struct globalfifo_dev *dev = filp->private_data;
  int ret = 0;
  DECLARE_WAITQUEUE(wait, current);
  mutex_lock(&dev->mutex);
  add_wait_queue(&dev->r_wq, &wait);

  while (dev->len == 0)
  {
    if (filp->f_flags & O_NONBLOCK)
    {
      ret = -EAGAIN;
      goto out_1;
    }
    __set_current_state(TASK_INTERRUPTIBLE);
    mutex_unlock(&dev->mutex);

    schedule();
    if (signal_pending(current))
    {
      ret = -ERESTARTSYS;
      goto out_2;
    }
    mutex_lock(&dev->mutex);
  }

  if (count > dev->len)
    count = dev->len;

  if (copy_to_user(buf, dev->mem, count) != 0)
    ret = -EFAULT;
  else
  {
    dev->len = dev->len - count;
    memcpy(dev->mem, dev->mem + count, dev->len);
    ret = count;
    wake_up_interruptible(&dev->w_wq);
  }

out_1:
  mutex_unlock(&dev->mutex);
out_2:
  remove_wait_queue(&dev->r_wq, &wait);
  set_current_state(TASK_RUNNING);
  return ret;
}

static ssize_t globalfifo_write_func(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
  int ret = -ENOMEM;
  struct globalfifo_dev *dev = filp->private_data;
  DECLARE_WAITQUEUE(wait, current);

  mutex_lock(&dev->mutex);
  add_wait_queue(&dev->w_wq, &wait);

  while (dev->len == GLOBALFIFO_SIZE)
  {
    if (filp->f_flags & O_NONBLOCK)
    {
      ret = -EAGAIN;
      goto out_1;
    }

    __set_current_state(TASK_INTERRUPTIBLE);

    mutex_unlock(&dev->mutex);

    schedule();
    if (signal_pending(current))
    {
      ret = -ERESTARTSYS;
      goto out_2;
    }
    mutex_lock(&dev->mutex);
  }
  if (count > GLOBALFIFO_SIZE - dev->len)
    count = GLOBALFIFO_SIZE - dev->len;

  if (copy_from_user(dev->mem + dev->len, buf, count))
    ret = -EFAULT;
  else
  {
    dev->len += count;
    wake_up_interruptible(&dev->r_wq);
    ret = count;
  }
out_1:
  mutex_unlock(&dev->mutex);
out_2:
  remove_wait_queue(&dev->w_wq, &wait);
  set_current_state(TASK_RUNNING);
  return ret;
}

static long globalfifo_ioctl_func(struct file *filp, unsigned int cmd, unsigned long arg)
{
  struct globalfifo_dev *dev = filp->private_data;
  int ret = 0;

  // 检查幻数(返回值POSIX标准规定，也用-EINVAL)
  if (_IOC_TYPE(cmd) != GLOBALFIFO_MAGIC)
    return -ENOTTY;
  // 检查命令编号
  if (_IOC_NR(cmd) > GLOBALFIFO_MAXNR)
    return -ENOTTY;
  // 检查命令方向，并验证用户空间指针的访问权限。
  if (_IOC_DIR(cmd) & _IOC_READ)
    ret = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
  else if (_IOC_DIR(cmd) & _IOC_WRITE)
    ret = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

  if (ret)
    return -EFAULT;

  switch (cmd)
  {
  case GLOBALFIFO_CLEAR:
    mutex_lock(&dev->mutex);
    dev->len = 0;
    memset(dev->mem, 0, GLOBALFIFO_SIZE);
    mutex_unlock(&dev->mutex);
    break;
  default:
    return -EINVAL;
  }
  return 0;
}

static const struct file_operations globalfifo_fops =
    {
        .owner = THIS_MODULE,
        .read = globalfifo_read_func,
        .write = globalfifo_write_func,
        .unlocked_ioctl = globalfifo_ioctl_func,
        .open = globalfifo_open_func,
        .release = globalfifo_release_func,
};

static int __init globalfifo_init_module(void)
{
  int ret = 0, i;
  dev_t devno = MKDEV(globalfifo_major, globalfifo_minor);

  if (globalfifo_major)
    ret = register_chrdev_region(devno, globalfifo_nr_devs, "globalfifo");
  else
  {
    ret = alloc_chrdev_region(&devno, globalfifo_minor, globalfifo_nr_devs, "globalfifo");
    globalfifo_major = MAJOR(devno);
  }

  if (ret < 0)
    return ret;

  globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev) * globalfifo_nr_devs, GFP_KERNEL);
  if (!globalfifo_devp)
  {
    printk(KERN_WARNING "alloc mem failed");
    ret = -ENOMEM;
    goto out_1;
  }

  globalfifo_cls = class_create(THIS_MODULE, "globalfifo");
  if (IS_ERR(globalfifo_cls))
  {
    printk(KERN_WARNING "Error creating class for ioctl");
    goto out_2;
  }

  for (i = 0; i < globalfifo_nr_devs; i++)
  {
    cdev_init(&globalfifo_devp[i].cdev, &globalfifo_fops);
    globalfifo_devp[i].cdev.owner = THIS_MODULE;
    ret = cdev_add(&globalfifo_devp[i].cdev, MKDEV(globalfifo_major, globalfifo_minor + i), 1);
    if (ret)
      printk(KERN_WARNING "Error adding cdev for device %d", i);
    else
    {
      globalfifo_devp[i].class_dev = device_create(globalfifo_cls, NULL, MKDEV(globalfifo_major, globalfifo_minor + i), NULL, "globalfifo%d", i);
      if (IS_ERR(globalfifo_devp[i].class_dev))
        printk(KERN_WARNING "Error creating device for device %d", i);
    }
    mutex_init(&globalfifo_devp[i].mutex);
    init_waitqueue_head(&globalfifo_devp[i].r_wq);
    init_waitqueue_head(&globalfifo_devp[i].w_wq);
  }
  return 0;
out_2:
  kfree(globalfifo_devp);
out_1:
  unregister_chrdev_region(devno, globalfifo_nr_devs);
  return ret;
}

static void __exit globalfifo_exit_module(void)
{
  int i;
  for (i = 0; i < globalfifo_nr_devs; i++)
  {
    device_destroy(globalfifo_cls, MKDEV(globalfifo_major, globalfifo_minor + i));
    cdev_del(&globalfifo_devp[i].cdev);
  }
  class_destroy(globalfifo_cls);
  kfree(globalfifo_devp);
  unregister_chrdev_region(MKDEV(globalfifo_major, globalfifo_minor), globalfifo_nr_devs);
  printk(KERN_INFO "globalfifo exit\n");
}

module_param(globalfifo_major, int, S_IRUGO);
module_param(globalfifo_minor, int, S_IRUGO);
module_param(globalfifo_nr_devs, int, S_IRUGO);

module_init(globalfifo_init_module);
module_exit(globalfifo_exit_module);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");
