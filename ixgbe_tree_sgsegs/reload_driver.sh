#!/bin/bash

echo "8 8 8" > /proc/sys/kernel/printk

rmmod ixgbe

# Example alternate invocations
#insmod ixgbe.ko drv_gso_size=16384
#insmod ixgbe.ko use_pkt_ring=1
#insmod ixgbe.ko
#insmod ixgbe.ko xmit_batch=1
insmod ixgbe.ko xmit_batch=1 num_queues=32

sleep 1

ethtool -G eth0 rx 4096 tx 4096

sleep 1

#ifconfig eth0 10.42.0.11
ifconfig eth0 10.10.1.3

sleep 2
echo "Starting test" > /dev/kmsg
