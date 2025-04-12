# File: src/Makefile
# -----------------------------------------
# This Makefile enables:
#  - Building the kernel modules out-of-tree ("make modules")
#  - Building the userspace program ("make" without arguments)
#
# Allows local testing without building the entire OpenWrt.

# Out-of-tree kernel build path
KDIR ?= /lib/modules/$(shell uname -r)/build

# Compiler + flags for userspace
CC ?= gcc
CFLAGS ?= -O2 -Wall
LIBS ?= -luci -lsysfs

# Kernel module sources (via Kbuild)
KBUILD_FILES = module_temp.c quectel_rm520n_temp_sensor.c quectel_rm520n_temp_sensor_hwmon.c

# Userspace program
TARGET = quectel_rm520n_temp_daemon
SRCS   = daemon.c quectel_rm520n_temp_daemon_sensor.c quectel_rm520n_temp_daemon_sensor_hwmon.c
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
