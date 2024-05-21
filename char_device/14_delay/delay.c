/*
 * @Date: 2024-05-13 19:44:57
 * @author: lidonghang-02 2426971102@qq.com
 * @LastEditTime: 2024-05-19 19:44:22
 */

#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/delay.h>

static int __init delay_init_module(void)
{
  unsigned long tmp_j, t;
  DECLARE_WAIT_QUEUE_HEAD(wait);

  printk(KERN_INFO "delay init\n");
  tmp_j = jiffies;
  t = tmp_j + msecs_to_jiffies(1000);

  printk(KERN_INFO "time_befpre() delay 1s\n");
  while (time_before(jiffies, t))
  {
  }

  // 等待队列延时
  printk(KERN_INFO "wait queue dealy 1s\n");
  wait_event_interruptible_timeout(wait, NULL, msecs_to_jiffies(1000));

  // 短延时
  printk(KERN_INFO "short delay 1s\n");
  mdelay(1000);

  // 休眠延时
  printk(KERN_INFO "sleep delay 1s\n");
  schedule_timeout_interruptible(msecs_to_jiffies(1000));

  return 0;
}

static void __exit delay_exit_module(void)
{
  printk(KERN_INFO "delay exit\n");
}

module_init(delay_init_module);
module_exit(delay_exit_module);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");
