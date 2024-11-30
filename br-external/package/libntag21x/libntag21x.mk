################################################################################
#
# libntag21x
#
################################################################################

LIBNTAG21X_VERSION = v1.0.3
LIBNTAG21X_SITE = https://github.com/libdriver/ntag21x.git
LIBNTAG21X_SITE_METHOD = git
LIBNTAG21X_GIT_SUBMODULES = YES
LIBNTAG21X_INSTALL_STAGING = YES
LIBNTAG21X_LICENSE = MIT
LIBNTAG21X_LICENSE_FILES = LICENSE

LIBNTAG21X_DEPENDENCIES = libgpiod

define LIBNTAG21X_BUILD_CMDS
    $(MAKE) -C $(@D)/project/raspberrypi4b all
endef

define LIBNTAG21X_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0644 $(@D)/src/driver_ntag21x.h $(STAGING_DIR)/usr/include/driver_ntag21x.h
    $(INSTALL) -D -m 0644 $(@D)/example/driver_ntag21x_basic.h $(STAGING_DIR)/usr/include/driver_ntag21x_basic.h
	$(INSTALL) -D -m 0644 $(@D)/interface/driver_ntag21x_interface.h $(STAGING_DIR)/usr/include/driver_ntag21x_interface.h
    $(INSTALL) -D -m 0755 $(@D)/project/raspberrypi4b/libntag21x.so* $(STAGING_DIR)/usr/lib
endef

define LIBNTAG21X_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/project/raspberrypi4b/libntag21x.so* $(TARGET_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/project/raspberrypi4b/ntag21x $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
