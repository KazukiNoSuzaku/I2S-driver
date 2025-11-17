# Makefile for I2S Driver, Daemon, and Library

# Kernel module build
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC
LDFLAGS = -shared

# Library
LIB_NAME = libi2s.so
LIB_VERSION = 1.0
LIB_SOURCES = libi2s.c
LIB_OBJECTS = $(LIB_SOURCES:.c=.o)

# Daemon
DAEMON_NAME = i2sd
DAEMON_SOURCES = i2sd.c
DAEMON_OBJECTS = $(DAEMON_SOURCES:.c=.o)

# Example
EXAMPLE_NAME = i2s_example
EXAMPLE_SOURCES = i2s_example.c
EXAMPLE_OBJECTS = $(EXAMPLE_SOURCES:.c=.o)

# Installation paths
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include
SYSTEMD_DIR = /etc/systemd/system

# Targets
.PHONY: all clean install uninstall module daemon library example

all: module daemon library example

# Kernel module
module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Daemon
daemon: $(DAEMON_NAME)

$(DAEMON_NAME): $(DAEMON_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

# Library
library: $(LIB_NAME)

$(LIB_NAME): $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

# Example application
example: $(EXAMPLE_NAME)

$(EXAMPLE_NAME): $(EXAMPLE_OBJECTS) $(LIB_NAME)
	$(CC) $(CFLAGS) -o $@ $(EXAMPLE_OBJECTS) -L. -li2s -lm

# Pattern rules
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(DAEMON_NAME) $(LIB_NAME) $(EXAMPLE_NAME)
	rm -f *.o *~

# Install
install: all
	# Install kernel module
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a
	
	# Install daemon
	install -D -m 755 $(DAEMON_NAME) $(DESTDIR)$(BINDIR)/$(DAEMON_NAME)
	
	# Install library
	install -D -m 755 $(LIB_NAME) $(DESTDIR)$(LIBDIR)/$(LIB_NAME).$(LIB_VERSION)
	ln -sf $(LIB_NAME).$(LIB_VERSION) $(DESTDIR)$(LIBDIR)/$(LIB_NAME)
	install -D -m 644 libi2s.h $(DESTDIR)$(INCLUDEDIR)/libi2s.h
	ldconfig
	
	# Install systemd service
	install -D -m 644 i2sd.service $(DESTDIR)$(SYSTEMD_DIR)/i2sd.service
	systemctl daemon-reload
	
	# Load kernel module
	modprobe i2s_driver || true

# Uninstall
uninstall:
	# Unload and remove kernel module
	rmmod i2s_driver || true
	rm -f /lib/modules/$(shell uname -r)/extra/i2s_driver.ko
	depmod -a
	
	# Remove daemon
	systemctl stop i2sd || true
	systemctl disable i2sd || true
	rm -f $(BINDIR)/$(DAEMON_NAME)
	rm -f $(SYSTEMD_DIR)/i2sd.service
	systemctl daemon-reload
	
	# Remove library
	rm -f $(LIBDIR)/$(LIB_NAME)*
	rm -f $(INCLUDEDIR)/libi2s.h
	ldconfig

# Kernel module objects
obj-m += i2s_driver.o

# Help
help:
	@echo "I2S Driver Build System"
	@echo "======================="
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build everything (default)"
	@echo "  module    - Build kernel module"
	@echo "  daemon    - Build system daemon"
	@echo "  library   - Build user space library"
	@echo "  example   - Build example application"
	@echo "  install   - Install all components"
	@echo "  uninstall - Uninstall all components"
	@echo "  clean     - Remove build artifacts"
	@echo ""
	@echo "Usage examples:"
	@echo "  make              # Build everything"
	@echo "  make install      # Install (requires root)"
	@echo "  sudo make install # Install with sudo"
	@echo "  make clean        # Clean build"

#
# i2sd.service - systemd service file for I2S daemon
#
# Save this as: i2sd.service
#

# [Unit]
# Description=I2S System Daemon
# After=network.target
# 
# [Service]
# Type=simple
# ExecStart=/usr/local/bin/i2sd -f
# ExecReload=/bin/kill -HUP $MAINPID
# Restart=on-failure
# RestartSec=5
# User=root
# Group=root
# 
# [Install]
# WantedBy=multi-user.target

#
# Quick Start Guide
#
# 1. Build:
#    make
#
# 2. Install (as root):
#    sudo make install
#
# 3. Start daemon:
#    sudo systemctl start i2sd
#    sudo systemctl enable i2sd
#
# 4. Run example:
#    ./i2s_example
#
# 5. Check daemon status:
#    sudo systemctl status i2sd
#
# 6. View logs:
#    sudo journalctl -u i2sd -f
