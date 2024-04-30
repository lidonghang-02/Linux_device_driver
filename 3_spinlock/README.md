- 运行
```bash
// 在root模式下
make
dmesg -C
insmod spinlock.ko
mknod /dev/spinlock c 232 0

cat /dev/spinlock
dmesg
echo "1" > /dev/spinlock
dmesg
cat /dev/spinlock
dmesg
```