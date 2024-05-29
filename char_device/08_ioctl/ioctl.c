/*
 * @Date: 2024-05-07 15:45:31
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-26 20:50:55
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/device.h>

#include <linux/semaphore.h>
#include <linux/mutex.h>

#include "ioctl.h"

#define LOCK_USE 1

#define IOCTL_SIZE 0x1000

#define IOCTL_MAJOR 0
#define IOCTL_MINOT 0
#define IOCTL_NR_DEVS 1

static int ioctl_major = IOCTL_MAJOR;
static int ioctl_minor = IOCTL_MINOT;
static int ioctl_nr_devs = IOCTL_NR_DEVS;

struct ioctl_dev
{
  struct cdev cdev;
  struct device *class_dev;
  unsigned int len;
  unsigned char buf[IOCTL_SIZE];

  struct semaphore sema;
  struct mutex mutex;
};
static struct class *ioctl_cls;
static struct ioctl_dev *ioctl_devp;

static int ioctl_open_func(struct inode *inode, struct file *filp)
{
  struct ioctl_dev *dev = container_of(inode->i_cdev, struct ioctl_dev, cdev);
  filp->private_data = dev;
  printk(KERN_INFO "ioctl_open\n");
  return 0;
}

static int ioctl_release_func(struct inode *inode, struct file *filp)
{
  printk(KERN_INFO "ioctl_release\n");
  return 0;
}

static ssize_t ioctl_read_func(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  struct ioctl_dev *dev = filp->private_data;
  int ret = 0;
  if (*f_pos >= dev->len)
  {
    ret = -ENOMEM;
    printk(KERN_INFO "read beyond end of device\n");
    goto out_err_1;
  }
  if (count > dev->len - *f_pos)
    count = dev->len - *f_pos;

#if (LOCK_USE == 0)
  if (down_interruptible(&dev->sema))
    return -ERESTARTSYS;
#endif
#if (LOCK_USE == 1)
  if (mutex_lock_interruptible(&dev->mutex))
    return -ERESTARTSYS;
#endif

  if (copy_to_user(buf, dev->buf + *f_pos, count))
    ret = -EFAULT;
  else
  {
    *f_pos += count;
    ret = count;
  }

#if (LOCK_USE == 0)
  up(&dev->sema);
#endif
#if (LOCK_USE == 1)
  mutex_unlock(&dev->mutex);
#endif

out_err_1:
  return ret;
}

static ssize_t ioctl_write_func(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
  struct ioctl_dev *dev = filp->private_data;
  int ret = 0;

  if (*f_pos >= dev->len)
  {
    printk(KERN_INFO "write beyond end of device\n");
    ret = -ENOMEM;
    goto out_err_1;
  }
  if (count > dev->len - *f_pos)
    count = dev->len - *f_pos;

#if (LOCK_USE == 0)
  if (down_interruptible(&dev->sema))
    return -ERESTARTSYS;
#endif
#if (LOCK_USE == 1)
  if (mutex_lock_interruptible(&dev->mutex))
    return -ERESTARTSYS;
#endif

  if (copy_from_user(dev->buf + *f_pos, buf, count))
    ret = -EFAULT;
  else
  {
    *f_pos += count;
    ret = count;
  }

#if (LOCK_USE == 0)
  up(&dev->sema);
#endif
#if (LOCK_USE == 1)
  mutex_unlock(&dev->mutex);

#endif

out_err_1:
  return ret;
}

static long ioctl_ioctl_func(struct file *filp, unsigned int cmd, unsigned long arg)
{
  struct ioctl_dev *dev = filp->private_data;
  int ret = 0;
  unsigned int tmp = 0;

  // 检查幻数(返回值POSIX标准规定，也用-EINVAL)
  if (_IOC_TYPE(cmd) != IOCTL_CHR_MAGIC)
    return -ENOTTY;
  // 检查命令编号
  if (_IOC_NR(cmd) > IOCTL_MAXNR)
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
  case IOCTL_CLEAN:
    memset(dev->buf, 0, dev->len);
    printk(KERN_INFO "clean ioctl_dev\n");
    break;
  case IOCTL_GET_LEN:
    if (put_user(dev->len, (unsigned int __user *)arg))
      ret = -EFAULT;
    printk(KERN_INFO "get ioctl_dev len %u\n", dev->len);
    break;
  case IOCTL_SET_LEN:
    if (!capable(CAP_SYS_ADMIN))
      return -EPERM;
    if (get_user(tmp, (unsigned int __user *)arg))
    {
      printk(KERN_INFO "get_user failed\n");
      ret = -EFAULT;
    }
    else
    {
      if (tmp > IOCTL_SIZE)
        tmp = IOCTL_SIZE;
      dev->len = tmp;
    }
    printk(KERN_INFO "set ioctl_dev len to %u\n", dev->len);
    break;
  default:
    return -ENOTTY;
  }

  return ret;
}

struct file_operations ioctl_fops = {
    .owner = THIS_MODULE,
    .open = ioctl_open_func,
    .release = ioctl_release_func,
    .read = ioctl_read_func,
    .write = ioctl_write_func,
    .unlocked_ioctl = ioctl_ioctl_func,
};

static int __init ioctl_init_module(void)
{
  int ret = 0;
  dev_t devno = MKDEV(ioctl_major, 0);
  int i;

  // 注册设备号
  if (ioctl_major)
    ret = register_chrdev_region(devno, ioctl_nr_devs, "ioctl");
  else
  {
    ret = alloc_chrdev_region(&devno, 0, ioctl_nr_devs, "ioctl");
    ioctl_major = MAJOR(devno);
  }

  if (ret < 0)
    return ret;

  ioctl_devp = kzalloc(sizeof(struct ioctl_dev) * ioctl_nr_devs, GFP_KERNEL);
  if (!ioctl_devp)
  {
    printk(KERN_WARNING "alloc mem failed");
    ret = -ENOMEM;
    goto out_err_1;
  }
  // 创建一个类
  ioctl_cls = class_create(THIS_MODULE, "ioctl");
  if (IS_ERR(ioctl_cls))
  {
    printk(KERN_WARNING "Error creating class for ioctl");
    goto out_err_2;
  }

  for (i = 0; i < ioctl_nr_devs; i++)
  {
    cdev_init(&ioctl_devp[i].cdev, &ioctl_fops);
    ioctl_devp[i].cdev.owner = THIS_MODULE;
    ret = cdev_add(&ioctl_devp[i].cdev, MKDEV(ioctl_major, i), 1);
    if (ret)
      printk(KERN_WARNING "fail add hc_dev%d", i);
    else
    {
      ioctl_devp[i].len = 0;
      ioctl_devp[i].class_dev = device_create(ioctl_cls, NULL, MKDEV(ioctl_major, i), NULL, "ioctl%d", i);
      if (IS_ERR(ioctl_devp[i].class_dev))
      {
        printk(KERN_NOTICE "Error creating device for ioctl%d", i);
      }
    }
#if (LOCK_USE == 0)
    sema_init(&ioctl_devp[i].sema, 1); // 初始化信号量
#elif (LOCK_USE == 1)
    mutex_init(&ioctl_devp[i].mutex); // 初始化互斥量
#endif
  }

  printk(KERN_INFO "ioctl module init\n");
  return 0;
out_err_2:
  kfree(ioctl_devp);
out_err_1:
  unregister_chrdev_region(devno, ioctl_nr_devs);
  return ret;
}

static void __exit ioctl_exit_module(void)
{
  int i;
  for (i = 0; i < ioctl_nr_devs; i++)
  {
    device_destroy(ioctl_cls, MKDEV(ioctl_major, i));
    cdev_del(&ioctl_devp[i].cdev);
  }
  class_destroy(ioctl_cls);

  kfree(ioctl_devp);
  unregister_chrdev_region(MKDEV(ioctl_major, 0), ioctl_nr_devs);
  printk(KERN_INFO "ioctl module exit\n");
}

module_param(ioctl_major, int, S_IRUGO);
module_param(ioctl_minor, int, S_IRUGO);
module_param(ioctl_nr_devs, int, S_IRUGO);

module_init(ioctl_init_module);
module_exit(ioctl_exit_module);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");
