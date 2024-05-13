KVERS = $(shell uname -r)

# Kernel modules
# obj-m += export_symb.o
obj-m += export_symbol.o test_export.o  


# Specify flags for the module compilation.
#EXTRA_CFLAGS=-g -O0

build: kernel_modules

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
