KDIR = /lib/modules/$(shell uname -r)/build

obj-m += my_completion_drv.o
all:
	make -C $(KDIR)  M=$(shell pwd) modules
clean:
	make -C $(KDIR)  M=$(shell pwd) clean
