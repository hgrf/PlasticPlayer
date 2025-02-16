################################################################################
#
# pp3-core
#
################################################################################

PP3_CORE_SITE = $(BR2_EXTERNAL_BR2RAUC_PATH)/package/pp3-core
PP3_CORE_SITE_METHOD = local

PP3_CORE_DEPENDENCIES = freetype libntag21x libndef ncurses

PP3_CORE_MAKE_OPTS = \
    CC="$(TARGET_CC)" \
    CXX="$(TARGET_CXX)"

define PP3_CORE_BUILD_CMDS
    $(MAKE) -C $(@D) $(PP3_CORE_MAKE_OPTS) pp3-core
endef

define PP3_CORE_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/pp3-core $(TARGET_DIR)/usr/bin/pp3-core
	$(INSTALL) -D -m 0755 $(@D)/librespot_event_handler.sh $(TARGET_DIR)/usr/bin/librespot_event_handler.sh
	$(INSTALL) -D -m 0644 $(@D)/wifi.conf $(TARGET_DIR)/etc/dbus-1/system.d/wifi.conf
endef

# font obtained from https://fonts.google.com/download, license: Apache 2.0
define PP3_CORE_INSTALL_INIT_SYSTEMD
	$(INSTALL) -D -m 0644 $(BR2_EXTERNAL_BR2RAUC_PATH)/package/pp3-core/pp3-core.service \
		$(TARGET_DIR)/usr/lib/systemd/system/pp3-core.service
	$(INSTALL) -D -m 0644 $(BR2_EXTERNAL_BR2RAUC_PATH)/package/pp3-core/MaterialSymbolsSharp_Filled-Regular.ttf \
		$(TARGET_DIR)/usr/share/fonts/truetype/material-symbols.ttf
endef

$(eval $(generic-package))
