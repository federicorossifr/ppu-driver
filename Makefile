PWD := $(shell pwd)
obj-m += ppu.o

all:
	make ARCH=riscv CROSS_COMPILE=$(CROSS) -C $(KERNEL) M=$(PWD) modules

user: ppu_user.c
	$(CROSS)gcc ppu_user.c -o ppu_user.o       

clean:
	make -C $(KERNEL) SUBDIRS=$(PWD) clean
