REMOTE_IP := 192.168.11.13
REMOTE_INSTALL_DIR := /mnt/onboard

X_TOOLS_DIR := $(HOME)/x-tools/arm-cortex_a8-linux-gnueabi
CC := $(X_TOOLS_DIR)/bin/arm-cortex_a8-linux-gnueabi-gcc

PKG_CONFIG_SYSROOT_DIR := ../root
PKG_CONFIG_LIBDIR := $(PKG_CONFIG_SYSROOT_DIR)/lib/pkgconfig

PROGRAM := paraanim

all: $(PROGRAM)

$(PROGRAM): paraanim.c fbutils.c font_8x8.c font_8x16.c
	export PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) ; \
	export PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) ; \
	$(CC) -o $(PROGRAM) $^ `pkg-config --cflags --libs tslib`

install: $(PROGRAM)
	curl -u root: -T $(PROGRAM) ftp://$(REMOTE_IP)$(REMOTE_INSTALL_DIR)/

clean:
	rm -f $(PROGRAM)
