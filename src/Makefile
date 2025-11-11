# Source Makefile for Quectel RM520N thermal management
# This Makefile is called by the main OpenWRT Makefile

# Out-of-tree kernel build path
KDIR ?= /lib/modules/$(shell uname -r)/build

# Compiler + flags for userspace
CC ?= gcc
CFLAGS ?= -O2 -Wall
CFLAGS += -Wall -Wextra -Iinclude
LIBS ?= -luci -lsysfs

# Userspace program
TARGET = quectel_rm520n_temp
SRCS   = main.c serial.c config.c logging.c temperature.c ui.c system.c cli.c daemon.c uci_config.c
OBJS   = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	# Clean userspace
	rm -f $(OBJS) $(TARGET)
	# Clean kernel build artifacts
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f modules.order Module.symvers
