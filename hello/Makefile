REMOTE_IP = 192.168.11.13
REMOTE_INSTALL_DIR = /mnt/onboard

CC = $(HOME)/x-tools/arm-cortex_a8-linux-gnueabi/bin/arm-cortex_a8-linux-gnueabi-gcc

PROGRAM = hello

all: $(PROGRAM)

install: $(PROGRAM)
	curl -u root: -T $(PROGRAM) ftp://$(REMOTE_IP)$(REMOTE_INSTALL_DIR)/

clean:
	rm -f $(PROGRAM)
