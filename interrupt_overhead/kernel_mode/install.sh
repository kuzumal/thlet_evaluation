make clean
make

sudo insmod thlet_intr.ko
sudo mknod /dev/thlet_intr c 250 0
sudo chmod 666 /dev/thlet_intr