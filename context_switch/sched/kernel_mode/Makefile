ifneq ($(KERNELRELEASE),)

obj-m += thlet_sched.o

else

# The default assumes you cloned this as part of firesim-software (FireMarshal)
LINUXSRC=/home/qxh/CHIPYARD_TEST/chipyard/software/firemarshal/boards/default/linux

KMAKE=make -C $(LINUXSRC) KBUILD_MODPOST_WARN=1 ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- M=$(PWD)

thlet_sched.ko: thlet_sched.c
	$(KMAKE)

clean:
	$(KMAKE) clean

endif
