#!/bin/bash

echo "8 8 8" > /proc/sys/kernel/printk

rmmod ixgbe

# Example alternate invocations
#insmod ixgbe.ko drv_gso_size=16384
#insmod ixgbe.ko use_pkt_ring=1
insmod ixgbe.ko

ethtool -G eth0 rx 4096 tx 4096
ifconfig eth0 10.42.0.11

sleep 2
echo "Starting test" > /dev/kmsg
