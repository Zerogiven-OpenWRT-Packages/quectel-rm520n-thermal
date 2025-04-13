# Main Makefile for building the Quectel RM520N Thermal Management package.

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME    := quectel-rm520n-thermal
PKG_VERSION := 1.0.0
PKG_RELEASE := 1

PKG_MAINTAINER     := CSoellinger
PKG_LICENSE        := GPL
PKG_COPYRIGHT_YEAR := $(shell date +%Y)

include $(INCLUDE_DIR)/package.mk

# --- Kernel package definition ---
define KernelPackage/quectel-rm520n-thermal
  SUBMENU:=Other modules
  TITLE:=Quectel RM520N Thermal Management Kernel Modules
  FILES:= \
    $(PKG_BUILD_DIR)/quectel_rm520n_temp.ko \
    $(PKG_BUILD_DIR)/quectel_rm520n_temp_sensor.ko \
    $(PKG_BUILD_DIR)/quectel_rm520n_temp_sensor_hwmon.ko
  AUTOLOAD:=$(call AutoLoad,50,quectel_rm520n_temp quectel_rm520n_temp_sensor quectel_rm520n_temp_sensor_hwmon)
  DEPENDS:=+libuci +libsysfs +kmod-hwmon-core
  PKGARCH:=all
endef

define KernelPackage/quectel-rm520n-thermal/description
  Kernel modules for monitoring and managing the Quectel RM520N modem temperature.
  Provides sysfs access, virtual thermal sensors, and hwmon integration.
endef

# --- Userspace package definition ---
define Package/quectel-rm520n-thermal
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Quectel RM520N Thermal Management Tools
  URL:=https://github.com/Zerogiven-OpenWRT-Packages/Quectel-RM520N-Thermal
  MAINTAINER:=$(PKG_MAINTAINER)
  DEPENDS:=+kmod-quectel-rm520n-thermal
  PKGARCH:=all
endef

define Package/quectel-rm520n-thermal/description
  Tools and configuration for managing the Quectel RM520N modem temperature.
  Includes:
   - Kernel modules for sysfs, thermal sensors, and hwmon integration
   - Userspace tool for AT-based temperature reading
   - Device Tree overlay for dynamic sensor registration
endef

# --- Build/Prepare ---
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)

	$(CP) ./src/* $(PKG_BUILD_DIR)/
	
  # Compile the Device Tree Overlay.
	$(LINUX_DIR)/scripts/dtc/dtc -I dts -O dtb -@ -o $(PKG_BUILD_DIR)/quectel_rm520n_temp_sensor.dtbo ./files/dts/quectel_rm520n_temp_sensor_overlay.dts
endef
		
		CROSS_COMPILE="$(TARGET_CROSS)" \
# --- Build/Compile ---
define Build/Compile
  # 1) Kernel modules via Kbuild
	$(MAKE) $(KERNEL_MAKE_FLAGS) -C $(LINUX_DIR) M=$(PKG_BUILD_DIR) modules \
		EXTRA_CFLAGS="-DPKG_NAME=\\\"$(PKG_NAME)\\\" \
		              -DPKG_TAG=\\\"$(PKG_VERSION)-r$(PKG_RELEASE)\\\" \
		              -DPKG_MAINTAINER=\\\"$(PKG_MAINTAINER)\\\" \
		              -DPKG_LICENSE=\\\"$(PKG_LICENSE)\\\" \
		              -DPKG_COPYRIGHT_YEAR=\\\"$(PKG_COPYRIGHT_YEAR)\\\"" \
		ARCH="$(LINUX_KARCH)" \
		CROSS_COMPILE="$(TARGET_CROSS)"

  # 2) Userspace program
	$(TARGET_CC) -o $(PKG_BUILD_DIR)/quectel_rm520n_temp_daemon \
		$(PKG_BUILD_DIR)/daemon.c \
		$(PKG_BUILD_DIR)/daemon_tty.c \
		$(PKG_BUILD_DIR)/daemon_sensor.c \
		$(PKG_BUILD_DIR)/daemon_sensor_hwmon.c \
		-DPKG_NAME=\"$(PKG_NAME)\" \
		-DPKG_TAG=\"$(PKG_VERSION)-r$(PKG_RELEASE)\" \
		-DPKG_MAINTAINER=\"$(PKG_MAINTAINER)\" \
		-DPKG_LICENSE=\"$(PKG_LICENSE)\" \
		-DPKG_COPYRIGHT_YEAR=\"$(PKG_COPYRIGHT_YEAR)\" \
		-I$(PKG_BUILD_DIR)/src \
		-luci -lsysfs
		-Wall -Wextra
endef

# --- Kernel install (kernel-specific package) ---
define KernelPackage/quectel-rm520n-thermal/install
	$(INSTALL_DIR) $(1)/lib/modules/$(LINUX_VERSION)

	$(INSTALL_BIN) $(PKG_BUILD_DIR)/quectel_rm520n_temp.ko \
	               $(1)/lib/modules/$(LINUX_VERSION)/

	$(INSTALL_BIN) $(PKG_BUILD_DIR)/quectel_rm520n_temp_sensor.ko \
	               $(1)/lib/modules/$(LINUX_VERSION)/

	$(INSTALL_BIN) $(PKG_BUILD_DIR)/quectel_rm520n_temp_sensor_hwmon.ko \
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
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/quectel_rm520n_temp_daemon \
	               $(1)/usr/bin/quectel_rm520n_temp_daemon

	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/quectel_rm520n_temp_sensor.dtbo \
                  $(1)/lib/firmware/quectel_rm520n_temp_sensor.dtbo
endef

define Package/quectel-rm520n-thermal/postinst
#!/bin/sh
mkdir -p /lib/firmware/quectel_rm520n
mv /lib/firmware/quectel_rm520n_temp_sensor.dtbo /lib/firmware/quectel_rm520n/quectel_rm520n_temp_sensor.dtbo

/etc/init.d/quectel_rm520n_thermal enable
/etc/init.d/quectel_rm520n_thermal start
exit 0
endef

# --- Evaluation ---
$(eval $(call KernelPackage,quectel-rm520n-thermal))
$(eval $(call BuildPackage,quectel-rm520n-thermal))
