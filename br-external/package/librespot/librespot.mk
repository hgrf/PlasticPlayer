################################################################################
#
# librespot
#
################################################################################

LIBRESPOT_VERSION = 4c3e14c61d35215c5eed38a45a6d13d421b04b83
LIBRESPOT_SITE = $(call github,hgrf,librespot,$(LIBRESPOT_VERSION))
LIBRESPOT_LICENSE = GPL-3.0
LIBRESPOT_LICENSE_FILES = LICENSE

LIBRESPOT_DEPENDENCIES = host-rustc host-pkgconf

LIBRESPOT_CARGO_ENV = \
	PKG_CONFIG_ALLOW_CROSS=1 \
	OPENSSL_LIB_DIR=$(HOST_DIR)/lib \
        OPENSSL_INCLUDE_DIR=$(HOST_DIR)/include

LIBRESPOT_BIN_DIR = target/$(RUSTC_TARGET_NAME)/release

LIBRESPOT_CARGO_BUILD_OPTS = --no-default-features \
		      --features=alsa-backend


define SPOTIFYD_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/$(LIBRESPOT_BIN_DIR)/librespot \
		$(TARGET_DIR)/usr/bin/librespot
endef

define LIBRESPOT_INSTALL_INIT_SYSTEMD
	$(INSTALL) -D -m 0644 $(BR2_EXTERNAL_BR2RAUC_PATH)/package/librespot/librespot.service \
		$(TARGET_DIR)/usr/lib/systemd/system/librespot.service
endef

$(eval $(cargo-package))
