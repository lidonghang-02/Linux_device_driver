# ioctl()命令

ioctl 是设备驱动程序中设备控制接口函数，一个字符设备驱动通常会实现设备打开、关闭、读、写等功能，在一些需要细分的情境下，如果需要扩展新的功能，通常以增设 ioctl() 命令的方式实现。

Linux建议以下面的格式定义ioctl()命令：

| 设备类型 | 序列号 | 方向 | 数据尺寸 |
| -- | -- | -- | -- |
| 8位 | 8位 | 2位 | 13/14位 |

- 设别类型（type）：是一个幻数，可以是0-0xff中的任意值。内核中的`ioctl-number.txt`给出了已经被使用的幻数。
- 序列号（nr）：可以为任意 unsigned char 型数据，取值范围 0~255，如果定义了多个 ioctl 命令，通常从 0 开始编号递增
- 方向（dir）：数据传送的方向，可以为 _IOC_NONE、_IOC_READ、_IOC_WRITE、_IOC_READ | _IOC_WRITE，分别指示了四种访问模式：无数据、读数据、写数据、读写数据
- 数据尺寸（size）：占据 13bit 或者 14bit（体系相关，arm 架构一般为 14 位），指定了 arg 的数据类型及长度

在内核中，我们可以使用下面几个宏来定义 ioctl 命令：
```c
// 分别对应不同的访问模式
#define _IO(type,nr)        _IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,size)  _IOC(_IOC_READ,(type),(nr),\       
                                (_IOC_TYPECHECK(size)))
#define _IOW(type,nr,size)  _IOC(_IOC_WRITE,(type),(nr),\
                                (_IOC_TYPECHECK(size)))
#define _IOWR(type,nr,size) _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),\
                                (_IOC_TYPECHECK(size)))

```

在ioctl()函数的实现中，可以使用下面的宏对命令进行检查：

```c
_IOC_TYPE(unsigned int cmd)     // 获取幻数
_IOC_NR(unsigned int cmd)       // 获取序列号
_IOC_DIR(unsigned int cmd)      // 获取命令方向
```



