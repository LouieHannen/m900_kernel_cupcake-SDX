ifndef PRJROOT
    $(error Cannot locate project environment)
endif

.PHONY: clean install

ifeq ($(MODULES_DPRAM),y)
subdir-y += dpram
#subdir-y += multipdp
endif
ifeq ($(MODULES_CAMERA),y)
subdir-y += camera
endif
ifeq ($(MODULES_TA),y)
subdir-y += ta
endif
ifeq ($(MODULES_WLAN),y)
subdir-y += wlan
endif
ifeq ($(MODULES_MULTIMEDIA),y)
subdir-y += cmm
subdir-y += g2d
subdir-y += g3d
subdir-y += jpeg
subdir-y += mfc
subdir-y += pp
subdir-y += rotator
endif
ifeq ($(FLASH_ONENAND),y)
subdir-y += xsr
subdir-y += rfs
endif
ifeq ($(MODULES_VIBETONZ),y)
subdir-y += vibetonz
endif
ifeq ($(MODULES_OPTJOY),y)
subdir-y += optjoystick
endif
ifeq ($(MODULES_BTGPIO),y)
subdir-y += btgpio
endif
ifeq ($(MODULES_PARAM),y)
subdir-y += param
endif



SUBDIRS := $(subdir-y)

DO_INSTALL_MODS = y

all: $(SUBDIRS)

$(SUBDIRS)::
	@$(MAKE) -C $@

install: $(SUBDIRS)
ifeq ($(strip $(DO_INSTALL_MODS)),y)
	@for i in `echo $(SUBDIRS)`; do \
		$(MAKE) --no-print-directory -C $$i $@ > /dev/null 2>&1; \
	done
endif
	@$(MAKE) --no-print-directory -C $(KDIR) modules_install

clean:
	@for i in `echo $(SUBDIRS)`; do \
		$(MAKE) -C $$i $@; \
	done
