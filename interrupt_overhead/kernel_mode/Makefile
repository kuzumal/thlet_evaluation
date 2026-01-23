
KERNELDIR := /lib/modules/`uname -r`/build
obj-m := thlet_intr.o
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
    
#$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) modules    

clean:
	rm -f .thlet_intr.*
	rm -f -r .tmp_versions
	rm -f *~
	rm -f thlet_intr.ko
	rm -f thlet_intr.o
	rm -f thlet_intr.mod.*
	rm -f linux.mdl
	rm -f Modules.symvers
    
install:
	./install.sh
    
