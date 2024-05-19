/*
 * @Date: 2024-05-01 09:44:20
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-19 19:55:47
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#define SEM_SIZE 0x1000
#define MEM_CLEAR 0x1
#define SEM_MAJOR 230

static int major = 230;
static int minor = 0;
struct semaphore ops_sem;

struct sem_dev
{
  struct cdev cdev;
  struct class* cls;
  struct device* class_dev;
  struct semaphore sem;
  unsigned char mem[SEM_SIZE];
};

struct sem_dev* sem_devp;

static int sem_open_func(struct inode* inode, struct file* filp)
{

  if (down_interruptible(&ops_sem) != 0)
    return -ERESTARTSYS;
  filp->private_data = sem_devp;
  return 0;
}

static int sem_release_func(struct inode* inode, struct file* filp)
{
  up(&ops_sem);
  return 0;
}

static long sem_ioctl_func(struct file* filp, unsigned int cmd, unsigned long arg)
{
  struct sem_dev* dev = filp->private_data;
  switch (cmd)
  {
  case MEM_CLEAR:
    memset(dev->mem, 0, SEM_SIZE);
    printk(KERN_INFO "Memory cleared\n");
    break;
  default:
    return -ENOTTY;
  }
  return 0;
}

static ssize_t sem_read_func(struct file* filp, char __user* buf, size_t size, loff_t* ppos)
{
  unsigned long p = *ppos;
  unsigned int count = size;
  int ret = 0;
  struct sem_dev* dev = filp->private_data;

  if (p >= SEM_SIZE)
    return 0;
  if (count > SEM_SIZE - p)
    count = SEM_SIZE - p;

  // 获取信号量，可以被信号打断，这时候返回非0
  if (down_interruptible(&dev->sem) != 0)
    return -ERESTARTSYS;

  if (copy_to_user(buf, dev->mem + p, count))
    ret = -EFAULT;
  else
  {
    *ppos += count;
    ret = count;
    printk(KERN_INFO "read %u bytes from %lu\n", count, p);
  }
  up(&dev->sem);
  return ret;
}

static ssize_t sem_write_func(struct file* filp, const char __user* buf, size_t size, loff_t* ppos)
{
  unsigned long p = *ppos;
  unsigned int count = size;
  int ret = 0;
  struct sem_dev* dev = filp->private_data;

  if (p >= SEM_SIZE)
    return -ENOMEM;
  if (count > SEM_SIZE - p)
    count = SEM_SIZE - p;

  if (down_interruptible(&dev->sem) != 0)
    return -ERESTARTSYS;

  if (copy_from_user(dev->mem + p, buf, count))
    ret = -EFAULT;
  else
  {
    ret = count;
    printk(KERN_INFO "wrote %u bytes to %lu\n", count, p);
  }
  up(&dev->sem);
  return ret;
}

static struct file_operations sem_ops =
{
    .owner = THIS_MODULE,
    .open = sem_open_func,
    .release = sem_release_func,
    .unlocked_ioctl = sem_ioctl_func,
    .read = sem_read_func,
    .write = sem_write_func,
};

static int __init sem_init_module(void)
{
  int ret = 0;
  dev_t devno = MKDEV(major, minor);
  printk(KERN_INFO "Semaphore init\n");

  if (major)
    ret = register_chrdev_region(devno, 1, "sem");
  else
  {
    ret = alloc_chrdev_region(&devno, minor, 1, "sem");
    major = MAJOR(devno);
  }
  if (ret < 0)
    return ret;

  sem_devp = kzalloc(sizeof(struct sem_dev), GFP_KERNEL);
  if (!sem_devp)
  {
    ret = -ENOMEM;
    goto out_err_1;
  }
  cdev_init(&sem_devp->cdev, &sem_ops);
  sem_devp->cdev.owner = THIS_MODULE;
  if (cdev_add(&sem_devp->cdev, devno, 1))
  {
    printk(KERN_ERR "cdev_add failed\n");
    ret = -ENOMEM;
    goto out_err_1;
  }

  sem_devp->cls = class_create(THIS_MODULE, "sem_cls");
  if (IS_ERR(sem_devp->cls))
  {
    printk(KERN_ERR "class_create failed\n");
    ret = PTR_ERR(sem_devp->cls);
    goto out_err_1;
  }

  sem_devp->class_dev = device_create(sem_devp->cls, NULL, devno, NULL, "sem_dev");
  if (IS_ERR(sem_devp->class_dev))
  {
    printk(KERN_ERR "device_create failed\n");
    ret = PTR_ERR(sem_devp->class_dev);
    goto out_err_2;
  }
  sema_init(&sem_devp->sem, 1);

  sema_init(&ops_sem, 1);
  return 0;

out_err_2:
  class_destroy(sem_devp->cls);
out_err_1:
  unregister_chrdev_region(devno, 1);
  return ret;
}

static void __exit sem_exit_module(void)
{
  printk(KERN_INFO "Semaphore exit\n");
  device_destroy(sem_devp->cls, MKDEV(major, minor));
  class_destroy(sem_devp->cls);
  kfree(sem_devp);
  unregister_chrdev_region(MKDEV(major, minor), 1);
}

module_param(major, int, S_IRUGO);

module_init(sem_init_module);
module_exit(sem_exit_module);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");
