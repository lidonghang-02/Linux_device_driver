# 导出符号

在Linux内核中，导出符号（Exported symbols）是指内核模块可以访问的符号，这些符号通常是函数或变量。当内核模块需要调用内核中定义的函数或访问内核中定义的变量时，这些函数或变量必须被导出。

导出的符号可以在`/proc/kallsyms`文件中查看

## 导出符号方法
可以使用下面的宏导出符号到内核符号表中：

1. **`EXPORT_SYMBOL(symbol_name)`**：这是用于导出符号的宏。任何使用此宏声明的符号都将被添加到内核的符号表中，以便其他模块可以引用它。
2. **`EXPORT_SYMBOL_GPL(symbol_name)`**：这是与EXPORT_SYMBOL类似的宏，但它仅导出符号给那些遵循GNU公共许可证（GPL）的模块。

## 使用导出的符号

在使用的模块的源文件中使用 **`extern`** 来声明要使用的导出符号，然后使用即可。

## 测试

```bash
make
dmesg -C

insmod export_symbol.ko
insmod test_export.ko

dmesg

```