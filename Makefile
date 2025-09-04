#
# Copyright (C) 2025 Christopher Sollinger
# This is free software, licensed under the GNU General Public License.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME    := quectel-rm520n-thermal
PKG_VERSION := 0.0.0
PKG_RELEASE := 1
BINARY_NAME := quectel_rm520n_temp

PKG_MAINTAINER     := CSoellinger
PKG_LICENSE        := GPL
PKG_COPYRIGHT_YEAR := $(shell date +%Y)

PKG_BUILD_DEPENDS := uci sysfsutils

include $(INCLUDE_DIR)/package.mk

# --- Kernel package definition ---
define KernelPackage/quectel-rm520n-thermal
  SUBMENU:=Other modules
  TITLE:=Quectel RM520N Thermal Management Kernel Modules
  FILES:= \
    $(PKG_BUILD_DIR)/kmod/quectel_rm520n_temp.ko \
    $(PKG_BUILD_DIR)/kmod/quectel_rm520n_temp_sensor.ko \
    $(PKG_BUILD_DIR)/kmod/quectel_rm520n_temp_sensor_hwmon.ko
  AUTOLOAD:=$(call AutoLoad,50,quectel_rm520n_temp quectel_rm520n_temp_sensor quectel_rm520n_temp_sensor_hwmon)
  DEPENDS:=+libuci +libsysfs +kmod-hwmon-core
  PKGARCH:=all
endef

define KernelPackage/quectel-rm520n-thermal/description
  Kernel modules for monitoring and managing the Quectel RM520N modem temperature.
  Provides sysfs access, virtual thermal sensors, and hwmon integration.
  Includes configurable temperature thresholds and millidegree precision.
endef

# --- Userspace package definition ---
define Package/quectel-rm520n-thermal
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Quectel RM520N Thermal Management Tools
  URL:=https://github.com/Zerogiven-OpenWRT-Packages/Quectel-RM520N-Thermal
  MAINTAINER:=$(PKG_MAINTAINER)
  DEPENDS:=+kmod-quectel-rm520n-thermal +libuci +libsysfs
  PKGARCH:=all
endef

define Package/quectel-rm520n-thermal/description
  Tools and configuration for managing the Quectel RM520N modem temperature.
  Includes:
   - Kernel modules for sysfs, thermal sensors, and hwmon integration
   - Combined daemon and CLI tool with subcommand interface (read/daemon/config)
   - Watch mode for continuous temperature monitoring
   - UCI-based configuration with automatic service reload
   - Configurable temperature parsing prefixes for different modem models
   - Temperature output in millidegrees (default) or degrees Celsius
endef

# --- Build/Prepare ---
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

# --- Build/Compile ---
define Build/Compile
  # 1) Kernel modules via Kbuild
	$(MAKE) $(KERNEL_MAKE_FLAGS) -C $(LINUX_DIR) M=$(PKG_BUILD_DIR) modules \
		EXTRA_CFLAGS="-DPKG_NAME=\\\"$(PKG_NAME)\\\" \
		              -DBINARY_NAME=\\\"$(BINARY_NAME)\\\" \
		              -DPKG_TAG=\\\"$(PKG_VERSION)-r$(PKG_RELEASE)\\\" \
		              -DPKG_MAINTAINER=\\\"$(PKG_MAINTAINER)\\\" \
		              -DPKG_LICENSE=\\\"$(PKG_LICENSE)\\\" \
		              -DPKG_COPYRIGHT_YEAR=\\\"$(PKG_COPYRIGHT_YEAR)\\\"" \
		ARCH="$(LINUX_KARCH)" \
		CROSS_COMPILE="$(TARGET_CROSS)"

  # 2) Combined daemon and CLI tool with UCI config
	$(TARGET_CC) $(TARGET_CFLAGS) -o $(PKG_BUILD_DIR)/$(BINARY_NAME) \
		$(PKG_BUILD_DIR)/main.c \
		$(PKG_BUILD_DIR)/serial.c \
		$(PKG_BUILD_DIR)/config.c \
		$(PKG_BUILD_DIR)/logging.c \
		$(PKG_BUILD_DIR)/temperature.c \
		$(PKG_BUILD_DIR)/ui.c \
		$(PKG_BUILD_DIR)/system.c \
		$(PKG_BUILD_DIR)/cli.c \
		$(PKG_BUILD_DIR)/daemon.c \
		$(PKG_BUILD_DIR)/uci_config.c \
		-DPKG_NAME=\"$(PKG_NAME)\" \
		-DBINARY_NAME=\"$(BINARY_NAME)\" \
		-DPKG_TAG=\"$(PKG_VERSION)-r$(PKG_RELEASE)\" \
		-DPKG_MAINTAINER=\"$(PKG_MAINTAINER)\" \
		-DPKG_LICENSE=\"$(PKG_LICENSE)\" \
		-DPKG_COPYRIGHT_YEAR=\"$(PKG_COPYRIGHT_YEAR)\" \
		$(TARGET_LDFLAGS) -luci -lsysfs




endef

# --- Kernel install (kernel-specific package) ---
define KernelPackage/quectel-rm520n-thermal/install
	$(INSTALL_DIR) $(1)/lib/modules/$(LINUX_VERSION)

	$(INSTALL_BIN) $(PKG_BUILD_DIR)/kmod/quectel_rm520n_temp.ko \
	               $(1)/lib/modules/$(LINUX_VERSION)/

	$(INSTALL_BIN) $(PKG_BUILD_DIR)/kmod/quectel_rm520n_temp_sensor.ko \
	               $(1)/lib/modules/$(LINUX_VERSION)/

	$(INSTALL_BIN) $(PKG_BUILD_DIR)/kmod/quectel_rm520n_temp_sensor_hwmon.ko \
	               $(1)/lib/modules/$(LINUX_VERSION)/
endef

# --- Userspace package install ---
define Package/quectel-rm520n-thermal/install
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/quectel_rm520n_thermal \
	                $(1)/etc/config/quectel_rm520n_thermal

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/quectel_rm520n_thermal \
	               $(1)/etc/init.d/quectel_rm520n_thermal

	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(BINARY_NAME) \
	               $(1)/usr/bin/$(BINARY_NAME)

endef

define Package/quectel-rm520n-thermal/postinst
#!/bin/sh
/etc/init.d/quectel_rm520n_thermal enable
echo "Quectel RM520N Thermal Management Service installed and enabled."
exit 0
endef

# --- Evaluation ---
$(eval $(call KernelPackage,quectel-rm520n-thermal))
$(eval $(call BuildPackage,quectel-rm520n-thermal))
