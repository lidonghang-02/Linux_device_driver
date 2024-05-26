# kernel_usb

- usb.c: 位于`linux/drivers/usb/core/`，这个文件注册了一个USB总线，并向USB总线中注册了三个驱动，分别是USB接口驱动、HUB驱动、USB设备驱动。

- linux-usb.h: 位于`linux/include/linux/usb.h`，这里因为重名改为`linus-usb.h`。文件中定义了USB设备驱动、urb请求块等结构体

- ch9.h: 位于`linux/include/uapi/linux/usb/ch9.h`，文件中定义了USB设备、USB接口、USB端点等结构体

