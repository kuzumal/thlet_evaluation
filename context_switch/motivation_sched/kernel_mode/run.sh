#!/bin/bash

make
sudo insmod ./thlet_switch_mod.ko
sleep 2
sudo rmmod thlet_switch_mod
sudo dmesg