## Linux的I2C驱动
Linux的I2C驱动体系结构主要由I2C核心、I2C总线驱动和I2C设备驱动组成。

- I2C核心是I2C驱动的核心部分，是I2C总线驱动和I2C设备驱动的中间枢纽，提供了I2C总线驱动和I2C设备驱动的注册、注销方法，I2C通信方法（Algorithm）上层的与具体适配器无关的代码以及探测设备、检测设备地址的上层代码等。
- I2C总线驱动是对I2C硬件体系结构中适配器端的实现，填充了`i2c_adapter`和`i2c_algorithm`结构体。
- I2C设备驱动是对I2C硬件体系结构中设备端的实现。

## 文件功能 
> i2c-dev.h在`Linux/include/linux/`
> 
> uapi-i2c-dev.h 是`Linux/uapi/linux/i2c-dev.h/i2c-dev.h`文件，为避免重名才改为uapi-i2c-dev.h
>
> 其余文件均在`linux/drivers/i2c`中

- i2c-core.c实现了I2C核心的功能
- i2c-dev.c实现了I2C适配器设备文件的功能，每一个I2C适配器都被分配一个设备。通过适配器访问设备时的主设备号都是89，次设备号为0～256。提供了通用的`open()`、`write()`、`read()`、`ioctl()`和`close()`等接口。
- i2c-tegra.c是NVIDIA Tegra平台I2C驱动的实现。

## i2c-dev.c文件分析
> i2c-dev.c可以看作是一个I2C设备驱动

`i2cdev_read`和`i2cdev_write`对应用户空间使用的`read()`和`write()`文件操作接口。这两个函数分别调用I2C核心的`i2c_master_recv()`和`i2c_master_send()`函数来构造一条I2C消息并引发适配器Algorithm通信函数的调用，来完成消息的传输。
但是大多数I2C设备的读写流程都不止一条消息，所以`i2cdev_read`和`i2cdev_write`不具备通用性。

对于多条消息的读写，在用户空间需要组织`i2c_msg`消息数组并调用`I2C_RDWR IOCTL`命令。

### 常用IOCTL命令
- I2C_SLAVE：设置从设备地址
- I2C_RETRIES：没有收到设备ACK情况下的重试次数，默认为1
- I2C_TIMEOUT：超时时间，单位为毫秒
- I2C_RDWR：读写操作

