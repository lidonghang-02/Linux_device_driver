#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <sound/core.h>
#include <linux/spi/spi.h>
#include <asm/uaccess.h>
#include <linux/timer.h>

#include <mach/hardware.h>
#include <mach/regs-gpio.h>

#include <linux/gpio.h>
#include <plat/gpio-cfg.h>

#include <linux/mtd/cfi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

/* 参考:
 * drivers\mtd\devices\mtdram.c
 * drivers/mtd/devices/m25p80.c
 */

static struct spi_device *spi_flash;

/*
 *
 */
// 读取设备的制造商ID和设备ID
void SPIFlashReadID(int *pMID, int *pDID)
{
    unsigned char tx_buf[4];
    unsigned char rx_buf[2];

    tx_buf[0] = 0x90;
    tx_buf[1] = 0;
    tx_buf[2] = 0;
    tx_buf[3] = 0;

    spi_write_then_read(spi_flash, tx_buf, 4, rx_buf, 2);

    *pMID = rx_buf[0];
    *pDID = rx_buf[1];
}

// 写操作使能/禁用
static void SPIFlashWriteEnable(int enable)
{
    unsigned char val = enable ? 0x06 : 0x04;
    spi_write(spi_flash, &val, 1);
}

// 读取状态寄存器
static unsigned char SPIFlashReadStatusReg1(void)
{
    unsigned char val;
    unsigned char cmd = 0x05;

    spi_write_then_read(spi_flash, &cmd, 1, &val, 1);

    return val;
}
static unsigned char SPIFlashReadStatusReg2(void)
{
    unsigned char val;
    unsigned char cmd = 0x35;

    spi_write_then_read(spi_flash, &cmd, 1, &val, 1);

    return val;
}

// 等待设备完成操作
static void SPIFlashWaitWhenBusy(void)
{
    // 检查忙位
    while (SPIFlashReadStatusReg1() & 1)
    {
        /* 休眠一段时间 */
        /* Sector erase time : 60ms
         * Page program time : 0.7ms
         * Write status reg time : 10ms
         */
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(HZ / 100); /* 休眠10MS后再次判断 */
    }
}

// 写入状态寄存器的值
static void SPIFlashWriteStatusReg(unsigned char reg1, unsigned char reg2)
{
    unsigned char tx_buf[4];

    SPIFlashWriteEnable(1);

    tx_buf[0] = 0x01;
    tx_buf[1] = reg1;
    tx_buf[2] = reg2;

    spi_write(spi_flash, tx_buf, 3);

    SPIFlashWaitWhenBusy();
}

// 下面两个清零操作需要根据具体的SPI Flash型号分析，这里我只是猜测其功能、

// 清除状态寄存器的写保护位
static void SPIFlashClearProtectForStatusReg(void)
{
    unsigned char reg1, reg2;

    reg1 = SPIFlashReadStatusReg1();
    reg2 = SPIFlashReadStatusReg2();

    // 将寄存器1和第7位和寄存器2的第0位清零
    reg1 &= ~(1 << 7);
    reg2 &= ~(1 << 0);

    SPIFlashWriteStatusReg(reg1, reg2);
}
// 清除状态寄存器的保护位
static void SPIFlashClearProtectForData(void)
{
    /* cmp=0,bp2,1,0=0b000 */
    unsigned char reg1, reg2;

    reg1 = SPIFlashReadStatusReg1();
    reg2 = SPIFlashReadStatusReg2();

    // 将寄存器1的第4、3、2位和寄存器2的第6位清零
    reg1 &= ~(7 << 2);
    reg2 &= ~(1 << 6);

    SPIFlashWriteStatusReg(reg1, reg2);
}

/* erase 4K */
void SPIFlashEraseSector(unsigned int addr)
{
    unsigned char tx_buf[4];
    tx_buf[0] = 0x20;
    tx_buf[1] = addr >> 16;
    tx_buf[2] = addr >> 8;
    tx_buf[3] = addr & 0xff;

    SPIFlashWriteEnable(1);

    spi_write(spi_flash, tx_buf, 4);

    SPIFlashWaitWhenBusy();
}

/* program */
// 写入数据
void SPIFlashProgram(unsigned int addr, unsigned char *buf, int len)
{
    unsigned char tx_buf[4];
    struct spi_transfer t[] = {
        {
            .tx_buf = tx_buf,
            .len = 4,
        },
        {
            .tx_buf = buf,
            .len = len,
        },
    };
    struct spi_message m;

    tx_buf[0] = 0x02;
    tx_buf[1] = addr >> 16;
    tx_buf[2] = addr >> 8;
    tx_buf[3] = addr & 0xff;

    SPIFlashWriteEnable(1);

    spi_message_init(&m);
    spi_message_add_tail(&t[0], &m);
    spi_message_add_tail(&t[1], &m);
    spi_sync(spi_flash, &m);

    SPIFlashWaitWhenBusy();
}
// 读操作
void SPIFlashRead(unsigned int addr, unsigned char *buf, int len)
{
    /* spi_write_then_read规定了tx_cnt+rx_cnt < 32
     * 所以对于大量数据的读取，不能使用该函数
     */

    unsigned char tx_buf[4];
    struct spi_transfer t[] = {
        {
            .tx_buf = tx_buf,
            .len = 4,
        },
        {
            .rx_buf = buf,
            .len = len,
        },
    };
    struct spi_message m;

    // 0x3应该是读取命令
    // tx_buf[1,2,3] 可能是存储addr的高，中，低字节
    tx_buf[0] = 0x03;
    tx_buf[1] = addr >> 16;
    tx_buf[2] = addr >> 8;
    tx_buf[3] = addr & 0xff;

    spi_message_init(&m);
    spi_message_add_tail(&t[0], &m);
    spi_message_add_tail(&t[1], &m);
    spi_sync(spi_flash, &m);
}

static void SPIFlashInit(void)
{
    SPIFlashClearProtectForStatusReg();
    SPIFlashClearProtectForData();
}

/* 构造注册一个mtd_info
 * mtd_device_register(master, parts, nr_parts)
 *
 */

/* 首先: 构造注册spi_driver
 * 然后: 在spi_driver的probe函数里构造注册mtd_info
 */

static struct mtd_info spi_flash_dev;

static int spi_flash_erase(struct mtd_info *mtd, struct erase_info *instr)
{
    // erase_info结构体，描述MTD设备的擦除操作信息
    // addr ：起始擦除地址
    // len ：擦除长度
    // state ：擦除状态
    // callback ：回调函数，当擦除操作完成时调用
    unsigned int addr = instr->addr;
    unsigned int len = 0;

    /* 判断参数 */
    // 检查起始地址和长度是否对齐于擦除块的边界
    if ((addr & (spi_flash_dev.erasesize - 1)) || (instr->len & (spi_flash_dev.erasesize - 1)))
    {
        printk("addr/len is not aligned\n");
        return -EINVAL;
    }

    for (len = 0; len < instr->len; len += 4096)
    {
        SPIFlashEraseSector(addr);
        addr += 4096;
    }

    instr->state = MTD_ERASE_DONE;
    // 调用回调函数
    mtd_erase_callback(instr);
    return 0;
}

static int spi_flash_read(struct mtd_info *mtd, loff_t from, size_t len,
                          size_t *retlen, u_char *buf)
{
    SPIFlashRead(from, buf, len);
    *retlen = len;
    return 0;
}

static int spi_flash_write(struct mtd_info *mtd, loff_t to, size_t len,
                           size_t *retlen, const u_char *buf)
{
    unsigned int addr = to;
    unsigned int wlen = 0;

    /* 判断参数 */
    if ((to & (spi_flash_dev.erasesize - 1)) || (len & (spi_flash_dev.erasesize - 1)))
    {
        printk("addr/len is not aligned\n");
        return -EINVAL;
    }

    for (wlen = 0; wlen < len; wlen += 256)
    {
        SPIFlashProgram(addr, (unsigned char *)buf, 256);
        addr += 256;
        buf += 256;
    }

    *retlen = len;
    return 0;
}

static int __devinit spi_flash_probe(struct spi_device *spi)
{
    int mid, did;

    spi_flash = spi;
    // 配置GPIO引脚
    s3c2410_gpio_cfgpin(spi->chip_select, S3C2410_GPIO_OUTPUT);
    SPIFlashInit();
    SPIFlashReadID(&mid, &did);
    printk("SPI Flash ID: %02x %02x\n", mid, did);
    memset(&spi_flash_dev, 0, sizeof(spi_flash_dev));
    /* 构造注册一个mtd_info
     * mtd_device_register(master, parts, nr_parts)
     *
     */

    /* Setup the MTD structure */
    /* 设置MTD结构 */
    /*
     * spi_flash_dev 是一个mtd_info结构体
     * mtd_info就结构体用于描述MTD设备的信息
     * MTD是一种用于访问易失性存储器（如Flash）的抽象层
     */
    spi_flash_dev.name = "100ask_spi_flash"; // 设备名称
    spi_flash_dev.type = MTD_NORFLASH;       // 设备类型，NOR Flash
    spi_flash_dev.flags = MTD_CAP_NORFLASH;  // 设备标志位，这里表示具有NOR Flash 特性和功能
    spi_flash_dev.size = 0x200000;           // 设备总大小 2M
    spi_flash_dev.writesize = 1;             // 单次写操作的大小为 1 字节
    spi_flash_dev.writebufsize = 4096;       // 写缓冲区大小,没有用到
    spi_flash_dev.erasesize = 4096;          // 擦除块的大小为4K

    spi_flash_dev.owner = THIS_MODULE;
    // 相关函数指针
    spi_flash_dev._erase = spi_flash_erase;
    spi_flash_dev._read = spi_flash_read;
    spi_flash_dev._write = spi_flash_write;

    // 注册MTD设备
    // 参数依次为 要注册的mtd_info结构体，MTD分区数组指针，分区的个数
    mtd_device_register(&spi_flash_dev, NULL, 0);

    return 0;
}

static int __devexit spi_flash_remove(struct spi_device *spi)
{
    mtd_device_unregister(&spi_flash_dev);
    return 0;
}

static struct spi_driver spi_flash_drv = {
    .driver = {
        .name = "100ask_spi_flash",
        .owner = THIS_MODULE,
    },
    .probe = spi_flash_probe,
    // __devexit_p 是一个宏，用于在 Linux 内核中标记函数为 "devexit" 函数。
    // "devexit" 函数是指在设备移除时调用的函
    .remove = __devexit_p(spi_flash_remove),
};

static int spi_flash_init(void)
{
    return spi_register_driver(&spi_flash_drv);
}

static void spi_flash_exit(void)
{
    spi_unregister_driver(&spi_flash_drv);
}

module_init(spi_flash_init);
module_exit(spi_flash_exit);
MODULE_DESCRIPTION("Flash SPI Driver");
MODULE_AUTHOR("weidongshan@qq.com,www.100ask.net");
MODULE_LICENSE("GPL");
