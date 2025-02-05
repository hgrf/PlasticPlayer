################################################################################
#
# rpi-utils
#
################################################################################

RPI_UTILS_VERSION = 33aa4da93d8da90d5075e8cd48e72cc90048b9f3
RPI_UTILS_SITE = $(call github,raspberrypi,utils,$(RPI_UTILS_VERSION))
RPI_UTILS_LICENSE = BSD-3-Clause
RPI_UTILS_LICENSE_FILES = LICENSE

$(eval $(cmake-package))
