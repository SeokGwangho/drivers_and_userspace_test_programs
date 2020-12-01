KDIR = /lib/modules/$(shell uname -r)/build

obj-m += hello_world_module.o
all:
	make -C $(KDIR)  M=$(shell pwd) modules
 
clean:
	make -C $(KDIR)  M=$(shell pwd) clean
