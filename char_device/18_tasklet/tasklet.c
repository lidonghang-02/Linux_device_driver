/*
 * @Date: 2024-05-18 11:12:53
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-19 19:57:13
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/sched.h>

#include <linux/interrupt.h>

#define TASKLET_MAJOR 0
#define TASKLET_MINOR 0
#define TASKLET_NR_DEVS 1

static int tasklet_major = TASKLET_MAJOR;
static int tasklet_minor = TASKLET_MINOR;
static int tasklet_nr_devs = TASKLET_NR_DEVS;

struct tasklet_dev
{
  struct cdev cdev;
  struct device* cls_dev;
  char* buf;
  int loops;
  unsigned long pre_jiffies;
  wait_queue_head_t wq;
  struct tasklet_struct tasklet;
};

static struct class* class = NULL;
static struct tasklet_dev* tasklet_devp;

static int tasklet_open_func(struct inode* inode, struct file* filp)
{
  struct tasklet_dev* dev = container_of(inode->i_cdev, struct tasklet_dev, cdev);
  filp->private_data = dev;

  printk(KERN_INFO "tasklet_open\n");
  return 0;
}

static int tasklet_release_func(struct inode* inode, struct file* filp)
{
  printk(KERN_INFO "tasklet_release\n");
  return 0;
}

static void tasklet_handler(unsigned long data)
{
  struct tasklet_dev* dev = (struct tasklet_dev*)data;
  dev->buf += sprintf(dev->buf,
    "%9li  %3li     %i    %6i   %i   %s\n",
    jiffies, jiffies - dev->pre_jiffies,
    in_interrupt() ? 1 : 0, current->pid,
    smp_processor_id(), current->comm);

  dev->loops--;
  if (dev->loops)
  {
    dev->pre_jiffies = jiffies;
    tasklet_schedule(&dev->tasklet);
  }
  else
    wake_up_interruptible(&dev->wq);
}
static ssize_t tasklet_read_func(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
  ssize_t ret = 0;
  size_t cnt;
  char* tmp_buf;

  struct tasklet_dev* dev = filp->private_data;

  // 只读一次
  if (*f_pos > 0)
    return 0;

  tmp_buf = kmalloc(200, GFP_KERNEL);
  if (!tmp_buf)
    return -ENOMEM;

  dev->loops = 4;
  dev->buf = tmp_buf;
  dev->buf += sprintf(dev->buf, "   time   delta  inirq    pid   cpu command\n");
  dev->buf += sprintf(dev->buf,
    "%9li  %3li     %i    %6i   %i   %s\n",
    jiffies, 0L, in_interrupt() ? 1 : 0,
    current->pid, smp_processor_id(), current->comm);
  dev->pre_jiffies = jiffies;

  tasklet_init(&dev->tasklet, tasklet_handler, (unsigned long)dev);
  tasklet_schedule(&dev->tasklet);

  wait_event_interruptible(dev->wq, !dev->loops);
  tasklet_kill(&dev->tasklet);

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

static const struct file_operations tasklet_fops = {
    .owner = THIS_MODULE,
    .open = tasklet_open_func,
    .release = tasklet_release_func,
    .read = tasklet_read_func,
};

static int __init tasklet_init_module(void)
{
  int ret, i;
  dev_t devno = MKDEV(tasklet_major, tasklet_minor);
  if (tasklet_major)
    ret = register_chrdev_region(devno, tasklet_nr_devs, "tasklet");
  else
  {
    ret = alloc_chrdev_region(&devno, 0, tasklet_nr_devs, "tasklet");
    tasklet_major = MAJOR(devno);
  }

  if (ret < 0)
  {
    printk(KERN_ALERT "register_chrdev_region failed with %d\n", ret);
    goto out_1;
  }

  tasklet_devp = kzalloc(tasklet_nr_devs * sizeof(struct tasklet_dev), GFP_KERNEL);
  if (!tasklet_devp)
  {
    ret = -ENOMEM;
    printk(KERN_ALERT "Out of memory\n");
    goto out_2;
  }

  class = class_create(THIS_MODULE, "tasklet");
  if (IS_ERR(class))
  {
    ret = PTR_ERR(class);
    printk(KERN_ALERT "class_create failed with %d\n", ret);
    goto out_3;
  }

  for (i = 0; i < tasklet_nr_devs; i++)
  {
    cdev_init(&tasklet_devp[i].cdev, &tasklet_fops);
    tasklet_devp[i].cdev.owner = THIS_MODULE;
    ret = cdev_add(&tasklet_devp[i].cdev, MKDEV(tasklet_major, tasklet_minor + i), 1);
    if (ret < 0)
      printk(KERN_ALERT "cdev_add failed with %d\n", ret);
    else
    {
      tasklet_devp[i].cls_dev = device_create(class, NULL, MKDEV(tasklet_major, tasklet_minor + i), NULL, "tasklet%d", i);
      if (IS_ERR(tasklet_devp[i].cls_dev))
        printk(KERN_ALERT "device_create failed with %ld\n", PTR_ERR(tasklet_devp[i].cls_dev));
    }
    init_waitqueue_head(&tasklet_devp[i].wq);
  }
  printk(KERN_INFO "tasklet_init\n");
  return 0;

out_3:
  kfree(tasklet_devp);
out_2:
  unregister_chrdev_region(devno, tasklet_nr_devs);
out_1:
  return ret;
}

static void __exit tasklet_exit_module(void)
{
  int i;
  for (i = 0; i < tasklet_nr_devs; i++)
  {
    device_destroy(class, MKDEV(tasklet_major, tasklet_minor + i));
    cdev_del(&tasklet_devp[i].cdev);
  }
  class_destroy(class);
  kfree(tasklet_devp);
  unregister_chrdev_region(MKDEV(tasklet_major, tasklet_minor), tasklet_nr_devs);
  printk(KERN_INFO "tasklet_exit\n");
}

module_param(tasklet_major, int, S_IRUGO);
module_param(tasklet_minor, int, S_IRUGO);
module_param(tasklet_nr_devs, int, S_IRUGO);

module_init(tasklet_init_module);
module_exit(tasklet_exit_module);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");
