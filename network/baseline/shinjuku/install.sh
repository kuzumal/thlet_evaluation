#!/bin/bash

export IMG=/home/qxh/CHIPYARD_TEST/chipyard/nana7mi_test/benchmark/riscv_linux/build/br-base-smallest.img
make clean && make
sudo mount $IMG mnt
sudo cp ./shinjuku_mod.ko mnt/root
ls mnt/root
sudo umount mnt