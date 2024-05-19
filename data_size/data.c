#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/utsname.h>

static int data_init_module(void)
{
    printk("char  short  int  long   ptr long-long  u8 u16 u32 u64\n");
    printk("%3i   %3i   %3i   %3i   %3i   %3i     %3i %3i %3i %3i\n",
           (int)sizeof(char), (int)sizeof(short), (int)sizeof(int),
           (int)sizeof(long), (int)sizeof(void *), (int)sizeof(long long),
           (int)sizeof(__u8), (int)sizeof(__u16), (int)sizeof(__u32),
           (int)sizeof(__u64));

    printk("le32:%x be32:%x htonl:%x ntohl:%x\n",
           cpu_to_le32(0x1234abcd), cpu_to_be32(0x1234abcd),
           htonl(0x1234abcd), ntohl(0x1234abcd));
    return 0;
}

static void data_exit_module(void)
{
}
module_init(data_init_module);
module_exit(data_exit_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lidonghang-02");
