- 测试
```bash
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