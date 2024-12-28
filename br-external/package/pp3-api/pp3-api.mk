################################################################################
#
# pp3-api
#
################################################################################

PP3_API_SITE = $(BR2_EXTERNAL_BR2RAUC_PATH)/package/pp3-api
PP3_API_SITE_METHOD = local

define PP3_API_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/main.py $(TARGET_DIR)/usr/lib/pp3-api/main.py
endef

define PP3_API_INSTALL_INIT_SYSTEMD
	$(INSTALL) -D -m 0644 $(BR2_EXTERNAL_BR2RAUC_PATH)/package/pp3-api/pp3-api.service \
		$(TARGET_DIR)/usr/lib/systemd/system/pp3-api.service
endef

$(eval $(generic-package))
