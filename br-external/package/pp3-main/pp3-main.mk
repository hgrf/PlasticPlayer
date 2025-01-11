################################################################################
#
# pp3-main
#
################################################################################

PP3_MAIN_SITE = $(BR2_EXTERNAL_BR2RAUC_PATH)/package/pp3-main
PP3_MAIN_SITE_METHOD = local

PP3_MAIN_DEPENDENCIES = libntag21x libndef ncurses

define PP3_MAIN_BUILD_CMDS
    $(TARGET_CXX) -L$(TARGET_DIR)/usr/lib -I$(STAGING_DIR)/usr/include -I$(STAGING_DIR)/usr/include/glib-2.0 -I$(STAGING_DIR)/usr/lib/glib-2.0/include -o $(@D)/pp3-main \
		$(@D)/main.cpp $(@D)/librespot.c $(@D)/ui.c $(@D)/wifistatus.c \
		-l:libntag21x.so.1.0.0 -lndef -lgpiod -lmenu -lncurses -lgio-2.0 -lglib-2.0 -lgobject-2.0
endef

define PP3_MAIN_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/pp3-main $(TARGET_DIR)/usr/bin/pp3-main
	$(INSTALL) -D -m 0755 $(@D)/librespot_event_handler.sh $(TARGET_DIR)/usr/bin/librespot_event_handler.sh
	$(INSTALL) -D -m 0644 $(@D)/wifi.conf $(TARGET_DIR)/etc/dbus-1/system.d/wifi.conf
endef

define PP3_MAIN_INSTALL_INIT_SYSTEMD
	$(INSTALL) -D -m 0644 $(BR2_EXTERNAL_BR2RAUC_PATH)/package/pp3-main/pp3-main.service \
		$(TARGET_DIR)/usr/lib/systemd/system/pp3-main.service
endef

$(eval $(generic-package))
