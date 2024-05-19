#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/string.h>

static char *str = "Hello, World!\n";
static int len;

// seq
static void *proc_seq_start(struct seq_file *m, loff_t *pos)
{
    printk(KERN_INFO "proc_seq_start\n");
    if (*pos >= len)
        return NULL;
    return &str[*pos];
}

static void proc_seq_stop(struct seq_file *m, void *v)
{
    printk(KERN_INFO "proc_seq_stop\n");
}

static int proc_seq_show(struct seq_file *m, void *v)
{
    printk(KERN_INFO "proc_seq_show\n");
    seq_putc(m, *(char *)v);
    return 0;
}

static void *proc_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
    printk(KERN_INFO "proc_seq_next\n");
    (*pos)++;
    if (*pos >= len)
        return NULL;
    return &str[*pos];
}

static const struct seq_operations proc_seq_ops = {
    .start = proc_seq_start,
    .stop = proc_seq_stop,
    .next = proc_seq_next,
    .show = proc_seq_show,
};

static int proc_open_func(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "proc_open_func\n");

    return seq_open(filp, &proc_seq_ops);
}

static ssize_t proc_read_func(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    int ret = 0;

    if (*f_pos < 0)
    {
        ret = -EINVAL;
        goto out;
    }
    if (*f_pos >= len)
        goto out;

    if (copy_to_user(buf, str, len))
    {
        ret = -EFAULT;
        goto out;
    }

    *f_pos += len;
    ret = len;

out:
    return ret;
}

static const struct file_operations proc_ops = {
    .open = proc_open_func,
    .read = proc_read_func,
};

static int __init proc_init(void)
{
    len = strlen(str);
    proc_create("my_proc", 0, NULL, &proc_ops);
    return 0;
}

static void __exit proc_exit(void)
{
    remove_proc_entry("my_proc", NULL);
    remove_proc_entry("my_proc_seq", NULL);
}

module_init(proc_init);
module_exit(proc_exit);

module_param(str, charp, S_IRUGO);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lidonghang-02");