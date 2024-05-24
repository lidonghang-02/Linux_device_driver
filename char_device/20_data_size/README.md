# 可移植性
## 关于跨平台可移植性
- 在编译时可以使用 `-Wall` 选项，启用几乎所有的警告。这有助于发现潜在的错误和不可移植的代码。
- 使用标准类型：使用<stdint.h>中定义的大小确定类型可以提高代码的可移植性。

##  内核对象数据类型
> 不同的架构，基础类型大小可能不同，主要区别在long和指针上

### 标准C语言类型

- char、short、int、long long在不同的平台上大小不变。
- long、ptr(指针)平台不同其大小不同，但二者的大小始终相同。
- char的符号问题：
    - 大多数平台上char默认是signed，但有些平台上默认是 unsigned。
    - char i = -1; 大部分平台上i是-1，有些平台上是255。
    - 应该使用：signed char i = -1;   unsigned char i = 255;

### 确定大小的类型
> <stdint.h>中定义
内核：当需要知道你定义的数据的大小时，可以使用内核提供的下列数据类型：
```c
u8; /* unsigned byte (8 bits) */
u16; /* unsigned word (16 bits) */
u32; /* unsigned 32-bit value */
u64; /* unsigned 64-bit value */

/*虽然很少需要有符号类型，但是如果需要，只要用 s 代替 u*/
```
用户空间 ： 若一个程序用户空间需要使用这些类型，可在符号前加一个双下划线 `__u8`（头文件linux/types.h）。


### 特定内核对象

在内核编程中，通常会使用一些特定于内核的类型来表示对象，如进程ID（pid_t）、文件描述符（fd_t）等。这些类型的大小也可能因平台而异，但它们通常由内核定义，并保证了在特定平台上的一致性。

## 字节序

### 大小端

大小端是指多字节数据在内存中的存储顺序。
大端模式（Big-Endian）：高位字节数据存放在低地址处，低位数据存放在高地址处
小端模式（Little-Endian）:高位字节数据存放在高地址处，低位数据存放在低地址处

给一个整形数据0x12345678
大端存储: 0x78  0x56  0x34  0x12  (高地址->低地址)
小端存储: 0x12  0x34  0x56  0x78  (高地址->低地址)


### 转换函数

为了在不同的大小端系统之间交换数据，需要使用一些转换函数。例如，cpu_to_le32()和le32_to_cpu()用于在小端系统和网络字节序（也是小端）之间进行转换；cpu_to_be32()和be32_to_cpu()用于在大端系统和网络字节序之间进行转换。另外，htonl()、ntohl()、htons()和ntohs()等函数也用于主机和网络字节序之间的转换。
>  在头文件<linux/byteorder.h>中
- `u32 cpu_to_le32 (u32)`：把cpu字节序转为小端字节序
- `u32 le32_to_cpu (u32)`：把小端字节序转为cpu字节序
- `u32 cpu_to_be32(u32)`：把cpu字节序转为大端字节序
- `u32 be32_to_cpu(u32)`：把大端字节序转为cpu字节序


下面函数通常用于网络字节序（通常是大端字节序）和主机字节序之间进行转换
- `uint32_t htonl(uint32_t hostlong)`：将主机字节序的32位无符号整数转换为网络字节序（大端）
- `uint16_t htons(uint16_t hostshort)`：将主机字节序的16位无符号整数转换为网络字节序（大端）
- `uint32_t ntohl(uint32_t netlong)`：将网络字节序的32位无符号整数转换为主机字节序
- `uint16_t ntohs(uint16_t netshort)`：将网络字节序的16位无符号整数转换为主机字节序（大端）

## 数据对齐

GNU C 通过 `__attribute__` 来声明 `aligned` 和 `packed` 属性，指定一个变量或类型的对齐方式。这两个属性用来告诉编译器：在给变量分配存储空间时，要按指定的地址对齐方式给变量分配地址。如果你想定义一个变量，在内存中以8字节地址对齐，就可以这样定义。
```c
int a __attribute__((aligned(8));
```
通过 `aligned` 属性，我们可以直接显式指定变量 a 在内存中的地址对齐方式。`aligned` 有一个参数，表示要按几字节对齐，使用时要注意地址对齐的字节数必须是2的幂次方，否则编译就会出错。

`packed` 是`__attribute__`的一个常见用法，当将`packed`属性应用于结构体或联合体时，它会消除结构体内的任何填充（padding），确保结构体或联合体的成员在内存中紧密排列，没有间隙。

例如：
```c
struct __attribute__((packed)) MyStruct {  
    char a;  
    int b;  
};
```

在上面的代码中，MyStruct结构体被标记为packed。如果没有packed属性，编译器可能会在char a和int b之间插入填充（padding），以确保int b按照其自然的对齐要求进行内存对齐。但是，由于packed属性的存在，编译器不会插入任何填充，因此char a和int b会紧密地连续存储在内存中。

## 其他问题
- 时间间隔:使用HZ代表一秒,不能假定每秒就1000个jiffies。
- 页大小：页大小为PAGE_SIZE,不要假设4K