#!/bin/bash

echo "8 8 8" > /proc/sys/kernel/printk

echo 1 > /proc/sys/net/ipv6/conf/all/disable_ipv6
#echo "Checking if ipv6 is disabled..."
#cat /proc/sys/net/ipv6/conf/eth0/disable_ipv6

rmmod ixgbe

# Example alternate invocations
#sudo insmod ixgbe.ko MQ=1,1 TSO=1,1
#sudo insmod ixgbe.ko MQ=1,1 TSO=1,1 XmitBatch=1,1
#sudo insmod ixgbe.ko MQ=1,1 TSO=1,1 UseSgseg=1,1 DrvGSOSize=1448,1448
sudo insmod ixgbe.ko MQ=1,1 TSO=1,1 XmitBatch=1,1 UseSgseg=1,1 DrvGSOSize=1448,1448

#sleep 1

ethtool -G eth0 rx 4096 tx 4096

#sleep 1

sleep 4

#ifconfig eth0 10.42.0.11
ifconfig eth0 10.10.1.3

sleep 2
echo "Starting test" > /dev/kmsg
