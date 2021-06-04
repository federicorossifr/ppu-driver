PWD := $(shell pwd)
obj-m += edu.o

all:
	make ARCH=riscv CROSS_COMPILE=$(CROSS) -C $(KERNEL) M=$(PWD) modules
clean:
	make -C $(KERNEL) SUBDIRS=$(PWD) clean
