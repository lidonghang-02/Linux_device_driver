# platform设备驱动

Linux 系统要考虑到驱动的可重用性，因此提出了驱动的分离与分层这样的软件思路，为了达到所有硬件都可以按照总线设备驱动模型来实现驱动，Linux从2.6起就加入了 platform 设备驱动，在内核中建立一条虚拟的总线platform，它可以将那些不依赖于传统总线（如PCI, USB, I2C等）的设备，虚拟的挂在了platform总线上，达到统一。

## platform_device结构体

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

## platform_driver结构体

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
- suspend 和 resume: 电源管理相关的回调，用于设备挂起和恢复。
- driver: 这是一个 struct device_driver 结构体，包含了驱动的一些通用信息。
- id_table: 往往一个驱动可能能同时支持多个硬件，这些硬件的名字都放在该结构体数组中。

## platform_bus_type

系统为platform总线创建了一个总线类型，platform_bus_type定义位于`Linux/drivers/base/platform.c`中
```c
struct bus_type platform_bus_type = {
        .name           = "platform",
        .dev_groups     = platform_dev_groups,
        .match          = platform_match,
        .uevent         = platform_uevent,
        .pm             = &platform_dev_pm_ops,
};
```
其中的`match()`函数实现了platform_device和platform_driver之间的匹配。

## 测试

```bash
make
insmod platform.ko
insmod platform-dev.ko
```
成功后可以看到如下目录结构
```bash
➜  my_platform_device git:(master) ✗ pwd
/sys/devices/platform/my_platform_device
➜  my_platform_device git:(master) ✗ ll
total 0
-rw-r--r-- 1 root root 4.0K  5月 19 15:56 driver_override
-r--r--r-- 1 root root 4.0K  5月 19 15:56 modalias
drwxr-xr-x 2 root root    0  5月 19 15:56 power
lrwxrwxrwx 1 root root    0  5月 19 15:56 subsystem -> ../../../bus/platform
-rw-r--r-- 1 root root 4.0K  5月 19 15:56 uevent

```

## 其他

关于platform的知识还有很多，这里只是简单的介绍和一种编写的方法，更多的知识在网上查阅，也可以参考内核中一些设备的实现，如DM9000网卡驱动（`/linux/drivers/net/ethernet/davicom/dm9000.c `）等