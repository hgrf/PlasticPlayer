################################################################################
#
# pp3-web-ui
#
################################################################################

PP3_WEB_UI_SITE = $(BR2_EXTERNAL_BR2RAUC_PATH)/package/pp3-web-ui
PP3_WEB_UI_SITE_METHOD = local

define PP3_WEB_UI_BUILD_CMDS
    rm -rf ${@D}/PlasticPlayer3/build
    rm -rf ${@D}/PlasticPlayer3/.dart_tool
    cd ${@D}/PlasticPlayer3 && /opt/flutter/bin/flutter build web
endef

define PP3_WEB_UI_INSTALL_TARGET_CMDS
    rsync -avh --delete ${@D}/PlasticPlayer3/build/web/ $(TARGET_DIR)/usr/html
endef

$(eval $(generic-package))
