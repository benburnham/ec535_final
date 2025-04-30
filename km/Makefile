# Ben Burnham and Mateen Sharif EC535 Spring 2025
# Adapted from nibbler example Makefile

ifneq ($(KERNELRELEASE),)
	obj-m := myco2.o
else
	KERNELDIR := $(EC535)/bbb/stock/stock-linux-4.19.82-ti-rt-r33
	PWD := $(shell pwd)
	ARCH := arm
	CROSS := arm-linux-gnueabihf-

# Cross-compile kernal module using specs above with $make
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) modules

# remove make files with $make clean
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) clean

endif
