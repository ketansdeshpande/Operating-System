ifeq ($(KERNELRELEASE),)
        # Assume the source tree is where the running kernel was built
        # You should set KERNELDIR in the environment if it's elsewhere
        KERNELDIR ?= /lib/modules/$(shell uname -r)/build
        # The current directory is passed to sub-makes as argument
        PWD := $(shell pwd)
all:
        $(MAKE) -C $(KERNELDIR) M=$(PWD) modules
        gcc -o producer producer.c
        gcc -o consumer consumer.c
#modules_install:
#       $(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install
clean:
        rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
.PHONY: modules modules_install clean
else
        # called from kernel build system: just declare what our modules are
        obj-m := device.o
endif