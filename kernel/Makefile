# Makefile for nxp_simtemp kernel module

obj-m += nxp_simtemp.o

# Kernel build directory
KDIR ?= /lib/modules/$(shell uname -r)/build

# Current directory
PWD := $(shell pwd)

# Default target
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

help:
	@echo "Targets:"
	@echo "  all     - Build the kernel module"
	@echo "  clean   - Remove build artifacts"
	@echo "  install - Install the module"

