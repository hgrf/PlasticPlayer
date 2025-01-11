################################################################################
#
# libndef
#
################################################################################

LIBNDEF_VERSION = 57ed905aab647d04137b39ba490df2b30c39579a
LIBNDEF_SITE = https://gitlab.com/RPiAwesomeness/libndef.git
LIBNDEF_SITE_METHOD = git
LIBNDEF_INSTALL_STAGING = YES
LIBNDEF_LICENSE = MIT
LIBNDEF_LICENSE_FILES = LICENSE

LIBNDEF_MAKE_OPTS = CXX="$(TARGET_CXX)"
LIBNDEF_MAKE_ENV = $(TARGET_MAKE_ENV)

define LIBNDEF_BUILD_CMDS
    $(LIBNDEF_MAKE_ENV) $(MAKE) -C $(@D) $(LIBNDEF_MAKE_OPTS) all
endef

define LIBNDEF_INSTALL_STAGING_CMDS
    mkdir -p $(STAGING_DIR)/usr/include/ndef-lite
    $(INSTALL) -D -m 0644 $(@D)/include/ndef-lite/*.hpp $(STAGING_DIR)/usr/include/ndef-lite/
    $(INSTALL) -D -m 0755 $(@D)/bin/libndef $(STAGING_DIR)/usr/lib/libndef.so
endef

define LIBNDEF_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/bin/libndef $(TARGET_DIR)/usr/lib/libndef.so
endef

$(eval $(generic-package))
