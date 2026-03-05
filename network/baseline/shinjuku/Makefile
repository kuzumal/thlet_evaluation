ifneq ($(KERNELRELEASE),)

obj-m += shinjuku_mod.o 

shinjuku_mod-y = shinjuku.o worker.o dispatcher.o context.o context_fast.o
else

# The default assumes you cloned this as part of firesim-software (FireMarshal)
LINUXSRC=/home/qxh/CHIPYARD_TEST/chipyard/software/firemarshal/boards/default/linux

KMAKE=make -C $(LINUXSRC) KBUILD_MODPOST_WARN=1 ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- M=$(PWD)

shinjuku.ko: shinjuku.c
	$(KMAKE)

clean:
	$(KMAKE) clean && cp ./context_fast.s.save ./context_fast.S
endif
