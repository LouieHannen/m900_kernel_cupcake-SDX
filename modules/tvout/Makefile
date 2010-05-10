#
#   Makefile for TVOUT 
#
#   Copyright(c) 2004-2006, Samsung Electronics, Co., Ltd.
#

obj-m   	:= s3c-tvscaler.o

obj-m		:= s3c-tvenc.o

all:
	@echo "$(MAKE) --no-print-directory -C $(KDIR) SUBDIRS=$(CURDIR) modules"
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules

clean:
	rm -f *.o *.ko *.mod.* *~ .*.cmd Module.symvers modules.order

install:
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules_install
