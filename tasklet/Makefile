KDIR = /lib/modules/$(shell uname -r)/build
obj-m += tasklet.o

all:
	make -C $(KDIR) M=$(PWD)

clean:
	make -C $(KDIR) M=$(PWD) clean
