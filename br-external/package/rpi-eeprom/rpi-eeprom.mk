################################################################################
#
# rpi-eeprom
#
################################################################################

RPI_EEPROM_VERSION = v2025.01.22-2712
RPI_EEPROM_SITE = $(call github,raspberrypi,rpi-eeprom,$(RPI_EEPROM_VERSION))
RPI_EEPROM_LICENSE = BSD-3-Clause
RPI_EEPROM_LICENSE_FILES = LICENSE

RPI_EEPROM_INSTALL = YES

# TODO: use latest firmware from that commit (check changelog)
# NOTE: current on my Rpi is 2023/01/11
# TODO: modify rpi-eeprom-update so that the correct boot partition is mounted (the one that is modified is the one that is loaded by uboot)
#       (or simply move the resulting files from /boot to the correct /mnt)
# TODO: automate the update of the EEPROM config


define RPI_EEPROM_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/rpi-eeprom-config $(TARGET_DIR)/bin/rpi-eeprom-config
	$(INSTALL) -D -m 0755 $(@D)/rpi-eeprom-digest $(TARGET_DIR)/bin/rpi-eeprom-digest
	$(INSTALL) -D -m 0755 $(@D)/rpi-eeprom-update $(TARGET_DIR)/bin/rpi-eeprom-update
	$(INSTALL) -D -m 0644 $(@D)/firmware-2711/default/pieeprom-2023-01-11.bin $(TARGET_DIR)/bin/firmware/default/pieeprom-2023-01-11.bin
	$(INSTALL) -D -m 0644 $(@D)/firmware-2711/default/recovery.bin $(TARGET_DIR)/bin/firmware/default/recovery.bin
endef

$(eval $(generic-package))
