ifneq ($(KERNELRELEASE),)

obj-m += concord_mod.o 

concord_mod-y = concord.o worker.o dispatcher.o context.o context_fast.o
RDTSC_PASS_PATH = /home/qxh/CHIPYARD_TEST/chipyard/nana7mi_test/benchmark/baseline/concord/src/cache-line-pass/build/src/libConcordPass.so
ccflags-y += -fpass-plugin=$(RDTSC_PASS_PATH)
else

# The default assumes you cloned this as part of firesim-software (FireMarshal)
LINUXSRC=/home/qxh/CHIPYARD_TEST/chipyard/software/firemarshal/boards/default/linux

KMAKE=make LLVM=-15 -C $(LINUXSRC) KBUILD_MODPOST_WARN=1 ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- M=$(PWD)

concord.ko: concord.c
	$(KMAKE)

clean:
	$(KMAKE) clean && cp ./context_fast.s.save ./context_fast.S
endif
