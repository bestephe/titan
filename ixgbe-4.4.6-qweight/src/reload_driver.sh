#!/bin/bash

sudo rmmod ixgbe
sudo insmod ixgbe.ko WRR=1,1 WrrCreditMult=4,4
sudo ifconfig p2p1 10.10.1.3 netmask 255.255.255.0
