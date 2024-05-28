# platform设备驱动

Linux 系统要考虑到驱动的可重用性，因此提出了驱动的分离与分层这样的软件思路，为了达到所有硬件都可以按照总线设备驱动模型来实现驱动，Linux从2.6起就加入了 platform 设备驱动，在内核中建立一条虚拟的总线platform，它可以将那些不依赖于传统总线（如PCI, USB, I2C等）的设备，虚拟的挂在了platform总线上，达到统一。

## platform总线驱动

platform总线驱动的代码可以查看linux/drivers/base/[platform.c](./kernel_platform/platform.c)文件



## platform 设备驱动

### 重要结构体

#### platform_device结构体

内核使用platform_device结构体来描述一个platform设备，该结构体定义位于`Linux/include/linux/platform_device.h`中

```c
struct platform_device
{
 	// 设备的名字，用于和驱动进行匹配的
	const char *name;
	// 内核中维护的所有的设备必须包含该成员
    struct device dev;
	//资源个数
    u32 num_resources;	
    //描述资源
    struct resource * resource;
    ...
};
```
这个结构体是为驱动程序提供设备信息，包括硬件信息和软件信息。

对于硬件信息，使用platform_device中的成员`resource`来描述，结构体如下

```c
struct resource {
    resource_size_t start;  // 资源的起始地址
    resource_size_t end;    // 资源的结束地址
    const char *name;       // 资源的名字
    unsigned long flags;    // 资源的类型
};
```
flags可以常见的有下面几种：
- IORESOURCE_IO：用于 IO 地址空间，对应于 IO 端口映射方式
- IORESOURCE_MEM：用于外设的可直接寻址的地址空间
- IORESOURCE_IRQ：用于指定该设备使用某个中断
- IORESOURCE_DMA：用于指定使用的 DMA 通道

`struct resource`一般按下面方法使用
```c
struct resource	res[]={
	[0] ={
		.start = 0x139d0000,
		.end  = 0x139d0000 + 0x3,
		.flags = IORESOURCE_MEM,
	},

	[1] ={
		.start = 199,
		.end  = 199,
		.flags = IORESOURCE_IRQ,
	},	
};
```


#### platform_driver结构体

这个结构体用来描述一个驱动

```c
struct platform_driver {  
    int (*probe)(struct platform_device *);  
    int (*remove)(struct platform_device *);  
    void (*shutdown)(struct platform_device *);  
    int (*suspend)(struct platform_device *, pm_message_t state);  
    int (*resume)(struct platform_device *);  
    struct device_driver driver;  
    const struct platform_device_id *id_table;  
    bool prevent_deferred_probe;
};

```
- probe: 当驱动和硬件信息匹配成功之后，就会调用probe函数，驱动所有的资源的注册和初始化全部放在probe函数中
- remove: 当设备被移除时，此函数被调用。
- shutdown: 系统关闭时，此函数被调用。
- suspend 和 resume: 电源管理相关的回调，用于设备挂起和恢复（已经过时）
- driver: 这是一个 struct device_driver 结构体，包含了驱动的一些通用信息。
- id_table: 往往一个驱动可能能同时支持多个硬件，这些硬件的名字都放在该结构体数组中。

### API

#### 注册/注销设备

```c
int platform_device_register(struct platform_device *pdev);
void platform_device_unregister(struct platform_device *pdev);
```

#### 注册/注销驱动API

```c
int platform_driver_register(struct platform_driver *drv);
void platform_driver_unregister(struct platform_driver *drv);
```

对于上面的函数，可以使用下面的宏替代

```c
module_platform_driver(platform_driver)
// platform_driver 是platform_driver结构体
```
这个宏会定义指定名称的平台设备驱动注册函数和平台设备驱动注销函数，并且在函数体内分别通过`platform_driver_register()`函数和`platform_driver_unregister()`函数注册和注销该平台设备驱动。

#### 获取resource

```c
struct resource *platform_get_resource(struct platform_device *dev,unsigned int type, unsigned int num);
```
- dev：指向要获取哪个设备的资源
- type：资源类型
- num：资源编号

## 框架

platform的驱动一般分为两个.c文件，一个是platform_driver.c文件，一个是platform_device.c文件。

在platform_device.c中，我们一般需要填充`struct platform_device`结构体和`struct resource`结构体，并使用`platform_device_register()`函数注册设备。这个文件必须先于
platform_driver.c文件完成，因为platform_driver.c文件中需要使用到`struct platform_device`结构体。

在platform_driver.c文件中，我们需要填充`platform_driver`结构体，platform_driver结构体中定义了probe函数，probe函数中会调用platform_get_resource函数获取设备资源，然后调用platform_set_drvdata函数将设备资源设置到设备结构体中。