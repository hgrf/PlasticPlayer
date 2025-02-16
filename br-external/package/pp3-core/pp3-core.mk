################################################################################
#
# pp3-core
#
################################################################################

PP3_CORE_SITE = $(BR2_EXTERNAL_BR2RAUC_PATH)/package/pp3-core
PP3_CORE_SITE_METHOD = local

PP3_CORE_DEPENDENCIES = freetype libntag21x libndef ncurses

define PP3_CORE_BUILD_CMDS
    $(TARGET_CXX) -L$(TARGET_DIR)/usr/lib -I$(STAGING_DIR)/usr/include -I$(STAGING_DIR)/usr/include/freetype2 -I$(STAGING_DIR)/usr/include/glib-2.0 -I$(STAGING_DIR)/usr/lib/glib-2.0/include -o $(@D)/pp3-core \
		$(@D)/main.cpp $(@D)/librespot.c $(@D)/tagreader.cpp $(@D)/ui.c $(@D)/icons.c $(@D)/bt.c $(@D)/wifistatus.c \
		-l:libntag21x.so.1.0.0 -lndef -lgpiod -lmenu -lncurses -lfreetype -lgio-2.0 -lglib-2.0 -lgobject-2.0
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
