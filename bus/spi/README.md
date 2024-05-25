# SPI

> SPI 驱动的框架与I2C类似，都是分为主机控制器驱动和设备驱动，我们需要实现的是具体设备的驱动，主机控制器的驱动由Linux内核实现，也需要了解
## 目录
```
kernel-4.14/drivers/spi/spi.c  Linux 提供的通用接口封装层驱动
kernel-4.14/drivers/spi/spidev.c  linux 提供的 SPI 通用设备驱动程序
kernel-4.14/include/linux/spi/spi.h  linux 提供的包含 SPI 的主要数据结构和函数
```

## SPI主机驱动

### 重要结构体
#### spi_master

Linux内核使用spi_master结构体表示spi主机驱动，spi_master结构体成员很多，具体内容可在在`include/linux/spi/spi.h`中查看，这里只列出部分成员。

```c
struct spi_master {
	struct device	dev; // 表示主机控制器的基本属性和操作的通用设备结构体。
	struct list_head list; //用于将主机控制器连接到 SPI 主机链表中的链表节点。
    ...
    //准备传输，设置传输的参数
    int         (*setup)(struct spi_device *spi);
    // 和 i2c_algorithm 中的 master_xfer 函数一样，控制器数据传输函数
	int			(*transfer)(struct spi_device *spi,
						struct spi_message *mesg);
	...    
    // 用于 SPI 数据发送，用于发送一个 spi_message，
    // SPI 的数据会打包成 spi_message，然后以队列方式发送出去
	int (*transfer_one_message)(struct spi_master *master,
				    struct spi_message *mesg);
	...
};

```

## SPI设备驱动

#### spi_device

Linux内核使用spi_device结构体表示spi设备

```c
struct spi_device {
	struct device	dev; // 代表该spi设备的device结构
	struct spi_master	*master; // 表示设备所属的主机控制器
    u32   max_speed_hz;//该设备最大传输速率
    u8    chip_select;//CS片选信号编号
    u8    bits_per_word;//每次传输长度
    u16   mode;//传输模式
    ...
    int   irq;//软件中断号
    void   *controller_state;//控制器状态
    void   *controller_data;//控制参数
    char   modalias[SPI_NAME_SIZE];//设备名称
    //CS 片选信号对应的 GPIO number
    int   cs_gpio;  /* chip select gpio */

    /* the statistics */
    struct spi_statistics statistics;
};
```

#### spi_driver

spi_driver表示一个 spi 设备驱动，当spi设备和驱动匹配成功后，preobe函数会执行

```c
struct spi_driver {
    //此driver所支持的 spi 设备 list
    const struct spi_device_id *id_table;
    int   (*probe)(struct spi_device *spi);
    int   (*remove)(struct spi_device *spi);
    //系统 shutdown 时的回调函数
    void   (*shutdown)(struct spi_device *spi);
    struct device_driver driver;
};
```


#### spi_transfer

当我们向 Linux 内核注册成功 spi_driver 以后就可以使用 SPI 核心层提供的 API 函数来对设备进行读写操作了。这步需要 spi_transfer 结构体，此结构体用于描述 SPI 传输的信息，是一次单个的SPI传输操作

```c
struct spi_transfer {
    const void *tx_buf; // 发送的数据
    void  *rx_buf;      // 接收的数据
    unsigned len;       // 传输数据的长度
    // SPI 是全双工通信，因此在一次通信中发送和接收的字节数都是一样的，
    // 所以 spi_transfer 中也就没有发送长度和接收长度之分

    dma_addr_t tx_dma;  //tx_buf 对应的 dma 地址
    dma_addr_t rx_dma;  //rx_buf 对应的 dma 地址
    struct sg_table tx_sg;
    struct sg_table rx_sg;

    //spi_transfer传输完成后是否要改变 CS 片选信号
    unsigned cs_change:1;
    unsigned tx_nbits:3;
    unsigned rx_nbits:3;
    ...
    u8  bits_per_word;  // 一个 word 占的bits
    u16  delay_usecs;   //两个 spi_transfer 之间的等待延迟
    u32  speed_hz;      // 传输速率

    struct list_head transfer_list;//spi_transfer挂载到的 message 节点
};
```

#### spi_message

spi_transfer 需要组织成 spi_message，即spi_message表示一组SPI传输操作
```c
struct spi_message {
    //挂载在此 msg 上的 transfer 链表头
    struct list_head transfers;
    //此 msg 需要通信的 spi 从机设备
    struct spi_device *spi;
    //所使用的地址是否是 dma 地址
    unsigned  is_dma_mapped:1;

    //msg 发送完成后的处理函数
    void   (*complete)(void *context);
    void   *context;//complete函数的参数
    unsigned  frame_length;
    unsigned  actual_length;//此 msg 实际成功发送的字节数
    int   status;//此 msg 的发送状态，0：成功，负数，失败

    struct list_head queue;//此 msg 在所有 msg 中的链表节点
    void   *state;//此 msg 的私有数据
};
```

### API

#### spi_driver注册/销毁

```c
int spi_register_driver(struct spi_driver *sdrv)
void spi_unregister_driver(struct spi_driver *sdrv);
```

#### spi_device注册/销毁

```c

struct spi_device *spi_new_device(struct spi_master *master,
								  struct spi_board_info *chip)
void spi_unregister_device
    (struct spi_device *spi);
```

#### spi_message

```c
// 初始化
void spi_message_init(struct spi_message *m)
// 添加spi_transfer到spi_message
void spi_message_add_tail(struct spi_transfer *t, struct spi_message *m)
```

#### 数据传输

```c
// 同步传输
int spi_sync(struct spi_device *spi, struct spi_message *message)
// 异步传输
int spi_async(struct spi_device *spi, struct spi_message *message)
```

同步和异步传输的区别在于，同步传输会阻塞等待传输完成，而异步传输不会等待传输完成。
异步传输需要设置 spi_message 中的 complete成员变量， complete 是一个回调函数，当 SPI 异步传输完成以后此函数就会被调用。


对于spi数据传输，大致步骤如下

1. 申请并初始化spi_transfer，设置 spi_transfer 的 tx_buf 、rx_buf 和 len 成员变量
2. 使用 spi_message_init 函数初始化 spi_message
3. 使用spi_message_add_tail函数将前面设置好的spi_transfer添加到spi_message队列中
4. 使用 spi_sync 或 spi_async 函数完成 SPI 数据同步传输。