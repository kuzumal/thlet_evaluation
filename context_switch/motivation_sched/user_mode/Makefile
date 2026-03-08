KERNELDIR := /lib/modules/`uname -r`/build
PWD := $(shell pwd)

obj-m := thlet_switch_helper_mod.o
thlet_switch_helper_mod-y := thlet_switch_helper.o
# ccflags-y := -O0

default:
	$(MAKE) -C $(KERNELDIR) KBUILD_MODPOST_WARN=1 M=$(PWD) modules
    
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean