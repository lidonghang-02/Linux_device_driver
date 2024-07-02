# Linux的I2C驱动

## 目录结构
- kernel_i2c: 里面文件来源与Linux/drivers/i2c
- read_code: 内核源码中实现的驱动实例，作为理解I2C的参考
- my_i2c: 自己实现的I2C驱动实例

## I2C体系结构

> 这部分内容是Linux内核实现的，我们需要实现的是具体I2C设备的驱动
```c
./drivers/i2c
├── algos 文件夹
├── busses 文件夹
├── i2c-boardinfo.c
├── i2c-core.c
├── i2c-core.h
├── i2c-dev.c
├── i2c-mux.c
├── i2c-slave-eeprom.c
├── i2c-smbus.c
├── i2c-stub.c
├── Kconfig
├── Makefile
└── muxes 文件夹
```
上面一些主要的文件功能：
- algos 文件夹：I2C通信算法
- busses 文件夹：I2C总线驱动相关的文件
- i2c-boardinfo.c：I2C设备信息
- i2c-core.c：实现I2C核心的功能
- i2c-dev.c：实现了I2C适配器设备文件的功能，每一个I2C适配器都被分配一个设备。通过适配器访问设备时的主设备号都是89，次设备号为0～256。提供通用的`open()`、`write()`、`read()`、`ioctl()`和`close()`等接口。


## 重要结构体

> 定义在include/linux/i2c.h中

- **`struct i2c_adapter`**：对应物理上的I2C适配器，代表一个I2C总线。一个适配器必须包括i2c_algorithm提供的通信函数来控制适配器产生特定的访问周期。
- **`struct i2c_algorithm`**：I2C通信算法，代表一个I2C通信算法。其中的`master_xfer()`函数用于产生I2C访问周期的信号，以i2c_msg为单位。
  - `i2c_msg`:I2C消息，在`./include/uapi/linux/i2c.h`中，定义了I2C传输地址、方向、缓冲区、缓冲区长度等信息
- **`struct i2c_driver`**：I2C驱动，代表一个I2C驱动程序，主要成员函数有`prebe()`,`remove()`,`suspend()`,`resume()`等。
- **`struct i2c_client`**：对应真实的物理设备，包括设备的一些描述信息，一个i2c_driver支持多个同类型的i2c_client

## I2C核心(i2c-core.c)

1. 增加/删除i2c_adapter
```c
int i2c_add_adapter(struct i2c_adapter *adapter);
void i2c_del_adapter(struct i2c_adapter *adap);
```
2. 增加/删除i2c_driver
```c
int i2c_register_driver(struct module *owner, struct i2c_driver *driver);
void i2c_del_driver(struct i2c_driver *driver)

// 下面宏位于i2.h中
#define i2c_add_driver(driver) \
	i2c_register_driver(THIS_MODULE, driver)
```
3. I2C传输，发送和接收
```c
// 适配器和设备之间的交互
int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);

// 下面两个函数是使用上面的transfer实现
int i2c_master_send(const struct i2c_client *client, const char *buf, int count);
int i2c_master_recv(const struct i2c_client *client, char *buf, int count);
```
i2c_transfer()函数并不具备驱动设备器物理硬件完成消息交互的能力，它只是将消息传递给i2c_adapter，由i2c_adapter调用i2c_algorithm的master_xfer()来完成消息的交互。

master_xfer()函数实际上就是产生I2C访问周期的start，stop，ack等信号

```c
// i2c-core.c文件1965行
ret = adap->algo->master_xfer(adap, msgs, num);
```


## i2c-dev.c文件分析

`i2cdev_read`和`i2cdev_write`对应用户空间使用的`read()`和`write()`文件操作接口。这两个函数分别调用I2C核心的`i2c_master_recv()`和`i2c_master_send()`函数来构造一条I2C消息并引发适配器Algorithm通信函数的调用，来完成消息的传输。
但是大多数I2C设备的读写流程都不止一条消息，所以`i2cdev_read`和`i2cdev_write`不具备通用性。

对于多条消息的读写，在用户空间需要组织`i2c_msg`消息数组并调用`I2C_RDWR IOCTL`命令。

### 常用IOCTL命令
- I2C_SLAVE：设置从设备地址
- I2C_RETRIES：没有收到设备ACK情况下的重试次数，默认为1
- I2C_TIMEOUT：超时时间，单位为毫秒
- I2C_RDWR：读写操作


## I2C适配器驱动

由于I2C总线控制器通常是在内存上的，所以它本身也连接在platform总线上，要通过platform_driver和platform_device的匹配来执行。

通常在I2C适配器驱动的init函数中，我们需要通过`platform_driver_register()`函数注册一个platform_driver驱动

在platform_driver的probe()函数中，完成下面工作
- 初始化适配器的硬件资源，如中断号，时钟，申请I/O地址等
- 初始化i2c_adapter结构体并使用i2c_add_adapter()函数注册

remove()函数与上面相反

在上面的初始化i2c_adapter结构体时，指定了i2c_algorithm通信算法，这里面实现了master_xfer函数


## I2C设备驱动

在I2C设备驱动的probe()函数中，完成下面工作
- 初始化i2c_client结构体并使用i2c_client_add()函数注册
- 初始化i2c_driver结构体并使用i2c_add_driver()函数注册

remove()函数与上面相反

