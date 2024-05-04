## 运行

```bash
**make**
insmod semaphore.ko

gcc test.c -o test
./test

// 另外开启一个终端
cat /dev/sem_dev
// 会发现只有当test结束后，才会输出内容
```