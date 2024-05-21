# 应用
> 创建10个同样globalmem设备，都在同一个globalmem类中，可以在`/sys/class/globalmem/`下看到
> 每个设备都使用互斥锁mutex
> 实现基本的open,release,read,write,ioctl,llseek操作
> 使用一个4k大小的mem存储数据
