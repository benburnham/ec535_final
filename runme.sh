#!/bin/sh

# Make the device node
mknod /dev/myco2 c 61 0
# Load the kernel module
insmod myco2.ko
