#!/bin/sh

# Make the device node
mknod /dev/mytraffic c 61 0
# Load the kernel module
insmod mytraffic.ko
