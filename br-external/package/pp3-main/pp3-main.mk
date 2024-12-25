################################################################################
#
# pp3-main
#
################################################################################

PP3_MAIN_SITE = $(BR2_EXTERNAL_BR2RAUC_PATH)/package/pp3-main
PP3_MAIN_SITE_METHOD = local

PP3_MAIN_DEPENDENCIES = libntag21x libndef ncurses

define PP3_MAIN_BUILD_CMDS
    $(CXX) -L$(TARGET_DIR)/usr/lib -I$(STAGING_DIR)/usr/include -o $(@D)/pp3-main \
		$(@D)/main.cpp $(@D)/librespot.c $(@D)/ui.c \
		-l:libntag21x.so.1.0.0 -lndef -lgpiod -lmenu -lncurses
endef

define PP3_MAIN_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/pp3-main $(TARGET_DIR)/usr/bin/pp3-main
	$(INSTALL) -D -m 0755 $(@D)/librespot_event_handler.sh $(TARGET_DIR)/usr/bin/librespot_event_handler.sh
endef

$(eval $(generic-package))
