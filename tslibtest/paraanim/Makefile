REMOTE_IP := 192.168.11.13
REMOTE_INSTALL_DIR := /mnt/onboard

X_TOOLS_DIR := $(HOME)/x-tools/arm-cortex_a8-linux-gnueabi
CC  := $(X_TOOLS_DIR)/bin/arm-cortex_a8-linux-gnueabi-gcc
CXX := $(X_TOOLS_DIR)/bin/arm-cortex_a8-linux-gnueabi-g++

PKG_CONFIG_SYSROOT_DIR := ../root
PKG_CONFIG_LIBDIR := $(PKG_CONFIG_SYSROOT_DIR)/lib/pkgconfig

CFLAGS   := $(shell PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) \
                    PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) \
                    pkg-config --cflags tslib)
CPPFLAGS := $(CFLAGS)
LDFLAGS  := $(shell PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) \
                    PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) \
                    pkg-config --libs tslib)

PROGRAM := paraanim

all: $(PROGRAM)

$(PROGRAM): paraanim.o fbutils.o font_8x8.o font_8x16.o

install: $(PROGRAM)
	curl -u root: -T $(PROGRAM) ftp://$(REMOTE_IP)$(REMOTE_INSTALL_DIR)/

clean:
	rm -f $(PROGRAM) *.o
