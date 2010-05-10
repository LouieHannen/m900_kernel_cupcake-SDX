##############################################################################
# COPYRIGHT(C)	: Samsung Electronics Co.Ltd, 2006-2011 ALL RIGHTS RESERVED
# AUTHOR		: KyoungHOON Kim (khoonk)
##############################################################################
# VERSION&DATE	: Version 1.00	2006/06/02 (khoonk)
##############################################################################

ifneq ($(KERNELRELEASE), )

EXTRA_CFLAGS :=	-I$(PRJROOT)/driver/include		\
				-I$(PRJROOT)/linux-$(LINUX_VERSION)/drivers/tbml/include	
EXTRA_CFLAGS += -DYAFFS_OUT_OF_TREE -DCONFIG_YAFFS_ON_BML -DCONFIG_YAFFS_YAFFS2 -DCONFIG_YAFFS_AUTO_YAFFS2 -DCONFIG_YAFFS_SHORT_NAMES_IN_RAM

obj-m := yaffs2.o
 
yaffs2-objs := yaffs_bmlif.o yaffs_bmlif2.o
yaffs2-objs += yaffs_mtdif1.o yaffs_packedtags1.o
yaffs2-objs += yaffs_ecc.o yaffs_fs.o yaffs_guts.o
yaffs2-objs += yaffs_packedtags2.o yaffs_qsort.o
yaffs2-objs += yaffs_tagscompat.o yaffs_tagsvalidity.o
yaffs2-objs += yaffs_checkptrw.o yaffs_nand.o
else

all:
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules

clean:
	rm -f *.o *.ko *.mod.c *~ .*.cmd Module.symvers

install:
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules_install

endif
