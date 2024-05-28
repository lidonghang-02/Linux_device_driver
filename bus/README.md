# bus



| | 项目 | I2C | SPI | USB | PCI |
| -- | -- | -- | -- | -- | -- |
| 主机测 | 描述主机的数据结构 | i2c_adapter | spi_master | usb_hcd | pci_bus | 
|  | 主机驱动传输成员函数 | master_xfer() | transfer() | urb_enqueue() | - |
|  | 主机的枚举方法 | 由I2C控制器挂接的总线决定(一般是platform) | 由SPI控制器挂接的总线决定(一般是platform) | 由USB控制器挂接的总线决定(一般是platform) | 扫描PCI总线上的设备|
| 核心层 | 描述传输协议的数据结构 | i2c_msg | spi_message | USB | 
|  | 传输API | i2c_transfer() | spi_sync() spi_async() | usb_submit_urb() | - |
| 外设端 | 外设的枚举方法 | i2c_driver | spi_driver | usb_driver | pci_driver |
|  | 描述外设的数据结构 | i2c_client | spi_device | usb_device | pci_dev |
| 板级逻辑 | 非设备树模式 | i2c_board_info | spi_board_info | 总线具备热插拔能力 |传统的内核配置和硬件描述|
|  | 设备树模式 | 在I2C控制器节点下添加子节点 | 在SPI控制器节点下添加子节点 | 总线具备热插拔能力 |通常不使用设备树|