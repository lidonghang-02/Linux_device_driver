KVERS = $(shell uname -r)

obj-m += wait_queue.o

build: kernel_modules 

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
	rm -rf .tmp_versions
