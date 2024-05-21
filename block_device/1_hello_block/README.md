# 测试


```bash
make

// 查看模块信息
modinfo hello_block.ko

// 加载模块
insmod hello_block.ko

// 查看模块
lsblk

// 格式化分区
mkfs -t ext4 /dev/hello_block

// 挂载
mkdir /mnt/hello_block
mount /dev/hello_block /mnt/hello_block

// 写入数据
echo "hello block device" > /mnt/hello_block/hello.txt

cat /mnt/hello_block/hello.txt

umount /mnt/hello_block

ls -al /mnt/hello_block

mount /dev/hello_block /mnt/hello_block

ls -al /mnt/hello_block
cat /mnt/hello_block/hello.txt

// 卸载
umount /mnt/hello_block
rmmod hello_block
make clean

```