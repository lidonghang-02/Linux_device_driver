// 目标：在华清远见的FS_S5PC100平台上编写一个简单的spi驱动模块，
// 在probe阶段实现对m25p10的ID号探测、
// flash擦除、flash状态读取、flash写入、flash读取等操作。
// 代码已经经过测试，运行于2.6.35内核。理解下面代码需要参照m25p10的芯片手册。
// 其实下面的代码和处理器没有太大关系，这也是spi子系统的分层特点。

#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/slab.h> // kzalloc
#include <linux/delay.h>

#define FLASH_PAGE_SIZE 256

/* Flash Operating Commands */
#define CMD_READ_ID 0x9f
#define CMD_WRITE_ENABLE 0x06
#define CMD_BULK_ERASE 0xc7
#define CMD_READ_BYTES 0x03
#define CMD_PAGE_PROGRAM 0x02
#define CMD_RDSR 0x05

/* Status Register bits. */
#define SR_WIP 1 /* Write in progress */
#define SR_WEL 2 /* Write enable latch */

/* ID Numbers */
#define MANUFACTURER_ID 0x20
#define DEVICE_ID 0x1120

/* Define max times to check status register before we give up. */
/* 定义放弃之前检查状态寄存器的最大次数。 */
#define MAX_READY_WAIT_COUNT 100000
#define CMD_SZ 4

struct m25p10a
{
    struct spi_device *spi;
    struct mutex lock;
    char erase_opcode;
    char cmd[CMD_SZ];
};

/*
 * Internal Helper functions
 */

/*
 * Read the status register, returning its value in the location
 * Return the status register value.
 * Returns negative if error occurred.
 */
/*
 * 读取状态寄存器，返回其所在位置的值
 * 返回状态寄存器值。
 * 如果发生错误则返回负数。
 */
static int read_sr(struct m25p10a *flash)
{
    ssize_t retval;
    u8 code = CMD_RDSR;
    u8 val;

    retval = spi_write_then_read(flash->spi, &code, 1, &val, 1);

    if (retval < 0)
    {
        dev_err(&flash->spi->dev, "error %d reading SR\n",
                (int)retval);
        return retval;
    }

    return val;
}

/*
 * Service routine to read status register until ready, or timeout occurs.
 * Returns non-zero if error.
 */
/*
 * 服务例程读取状态寄存器直到准备好，或者发生超时。
 * 如果出错或超时则返回1。
 */
static int wait_till_ready(struct m25p10a *flash)
{
    int count;
    int sr;

    /* one chip guarantees max 5 msec wait here after page writes,
     * but potentially three seconds (!) after page erase.
     */
    /* 一个芯片保证在页面写入后最多等待 5 毫秒，
     * 但可能在页面擦除后三秒（！）。
     */
    for (count = 0; count < MAX_READY_WAIT_COUNT; count++)
    {
        if ((sr = read_sr(flash)) < 0)
            break;
        else if (!(sr & SR_WIP))
            return 0;

        /* REVISIT sometimes sleeping would be best */
    }
    printk("in (%s): count = %d\n", count);

    return 1;
}

/*
 * Set write enable latch with Write Enable command.
 * Returns negative if error occurred.
 */
/*
 * 使用写使能命令设置写使能锁存器。
 * 如果发生错误则返回负数。
 */
static inline int write_enable(struct m25p10a *flash)
{
    flash->cmd[0] = CMD_WRITE_ENABLE;
    return spi_write(flash->spi, flash->cmd, 1);
}

/*
 * Erase the whole flash memory
 * 擦除整个闪存
 *
 * Returns 0 if successful, non-zero otherwise.
 */
static int erase_chip(struct m25p10a *flash)
{
    /* Wait until finished previous write command. */
    /* 等待直到完成之前的写入命令。 */
    if (wait_till_ready(flash))
        return -1;

    /* Send write enable, then erase commands. */
    write_enable(flash);
    flash->cmd[0] = CMD_BULK_ERASE;
    // 同步写，在spi.h中可以找到函数原型
    return spi_write(flash->spi, flash->cmd, 1);
}

/*
 * Read an address range from the flash chip.  The address range
 * may be any size provided it is within the physical boundaries.
 */
/*
 * 从Flash芯片中读取一个地址范围。  地址范围
 * 可以是任何尺寸，只要它在物理边界内。
 */
static int m25p10a_read(struct m25p10a *flash, loff_t from,
                        size_t len, char *buf)
{
    int r_count = 0, i;

    flash->cmd[0] = CMD_READ_BYTES;
    flash->cmd[1] = from >> 16;
    flash->cmd[2] = from >> 8;
    flash->cmd[3] = from;

#if 1
    struct spi_transfer st[2];
    struct spi_message msg;

    spi_message_init(&msg);
    memset(st, 0, sizeof(st));

    flash->cmd[0] = CMD_READ_BYTES;
    flash->cmd[1] = from >> 16;
    flash->cmd[2] = from >> 8;
    flash->cmd[3] = from;

    st[0].tx_buf = flash->cmd;
    st[0].len = CMD_SZ;
    spi_message_add_tail(&st[0], &msg);

    st[1].rx_buf = buf;
    st[1].len = len;
    spi_message_add_tail(&st[1], &msg);

    mutex_lock(&flash->lock);

    /* Wait until finished previous write command. */
    /* 等待直到完成之前的写入命令。 */
    if (wait_till_ready(flash))
    {
        mutex_unlock(&flash->lock);
        return -1;
    }

    spi_sync(flash->spi, &msg);
    r_count = msg.actual_length - CMD_SZ;
    printk("in (%s): read %d bytes\n", __func__, r_count);
    for (i = 0; i < r_count; i++)
    {
        printk("0x%02x\n", buf[i]);
    }

    mutex_unlock(&flash->lock);
#endif

    return 0;
}

/*
 * Write an address range to the flash chip.  Data must be written in
 * FLASH_PAGE_SIZE chunks.  The address range may be any size provided
 * it is within the physical boundaries.
 */
/*
 * 向Flash芯片写入一个地址范围。  数据必须写入
 * FLASH_PAGE_SIZE 块。  地址范围可以是提供的在物理边界内的任何大小
 */
static int m25p10a_write(struct m25p10a *flash, loff_t to,
                         size_t len, const char *buf)
{
    int w_count = 0, i, page_offset;
    struct spi_transfer st[2];
    struct spi_message msg;
#if 1
    if (wait_till_ready(flash))
    { // 读状态，等待ready
        mutex_unlock(&flash->lock);
        return -1;
    }
#endif
    write_enable(flash); // 写使能

    spi_message_init(&msg);
    memset(st, 0, sizeof(st));

    flash->cmd[0] = CMD_PAGE_PROGRAM;
    flash->cmd[1] = to >> 16;
    flash->cmd[2] = to >> 8;
    flash->cmd[3] = to;

    st[0].tx_buf = flash->cmd;
    st[0].len = CMD_SZ;
    spi_message_add_tail(&st[0], &msg);

    st[1].tx_buf = buf;
    st[1].len = len;
    spi_message_add_tail(&st[1], &msg);

    mutex_lock(&flash->lock);

    /* get offset address inside a page */
    /* 获取页面内的偏移地址 */
    page_offset = to % FLASH_PAGE_SIZE;

    /* do all the bytes fit onto one page? */
    /* 所有字节都适合一页吗？ */
    if (page_offset + len <= FLASH_PAGE_SIZE)
    { // yes
        st[1].len = len;
        printk("%d, cmd = %d\n", st[1].len, *(char *)st[0].tx_buf);
        // while(1)
        {
            spi_sync(flash->spi, &msg);
        }
        w_count = msg.actual_length - CMD_SZ;
    }
    else
    { // no
    }
    printk("in (%s): write %d bytes to flash in total\n", __func__, w_count);
    mutex_unlock(&flash->lock);
    return 0;
}

static int check_id(struct m25p10a *flash)
{
    char buf[10] = {0};
    flash->cmd[0] = CMD_READ_ID;
    // SPI 同步写入后读取
    // 写入cmd，读取buf
    spi_write_then_read(flash->spi, flash->cmd, 1, buf, 3);
    printk("Manufacture ID: 0x%x\n", buf[0]);
    printk("Device ID: 0x%x\n", buf[1] | buf[2] << 8);
    return buf[2] << 16 | buf[1] << 8 | buf[0];
}

static int m25p10a_probe(struct spi_device *spi)
{
    int ret = 0;
    struct m25p10a *flash;
    char buf[256];
    printk("%s was called\n", __func__);
    flash = kzalloc(sizeof(struct m25p10a), GFP_KERNEL);
    if (!flash)
    {
        return -ENOMEM;
    }
    flash->spispi = spi;
    mutex_init(&flash->lock);
    /* save flash as driver's private data */
    spi_set_drvdata(spi, flash);

    check_id(flash); // 读取ID
#if 1
    ret = erase_chip(flash); // 擦除
    if (ret < 0)
    {
        printk("erase the entirely chip failed\n");
    }
    printk("erase the whole chip done\n");
    memset(buf, 0x7, 256);
    m25p10a_write(flash, 0, 20, buf); // 0地址写入20个7
    memset(buf, 0, 256);
    m25p10a_read(flash, 0, 25, buf); // 0地址读出25个数
#endif
    return 0;
}

static int m25p10a_remove(struct spi_device *spi)
{
    return 0;
}

static struct spi_driver m25p10a_driver = {
    .probe = m25p10a_probe,
    .remove = m25p10a_remove,
    // driver是device_driver结构体，
    // 表示驱动程序的基本属性
    .driver = {
        .name = "m25p10a",
    },
};

static int __init m25p10a_init(void)
{
    return spi_register_driver(&m25p10a_driver);
}

static void __exit m25p10a_exit(void)
{
    spi_unregister_driver(&m25p10a_driver);
}

module_init(m25p10a_init);
module_exit(m25p10a_exit);

MODULE_DESCRIPTION("m25p10a driver for FS_S5PC100");
MODULE_LICENSE("GPL");
