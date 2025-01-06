#!/bin/sh

set -u
set -e

RAUC_COMPATIBLE="${2:-br2rauc-rpi4-64}"
BOARD_DIR="$(dirname $0)"
BOARD_NAME="$(basename ${BOARD_DIR})"
# Pass VERSION as an environment variable (eg: export from a top-level Makefile)
# If VERSION is unset, fallback to the Buildroot version
RAUC_VERSION=${VERSION:-${BR2_VERSION_FULL}}

# Add rpi-firmware device tree for boot
#
# NOTE: uboot is configured so that the device tree is "provided by the board (e.g.
# a previous loader) at runtime" (CONFIG_OF_BOARD). We want to use the rpi-firmware
# device tree here, but BR2_PACKAGE_RPI_FIRMWARE_INSTALL_DTBS depends on
# !BR2_LINUX_KERNEL_DTS_SUPPORT. This is a workaround to get the device tree into
# the boot partition.
cp ${BR2_EXTERNAL_BR2RAUC_PATH}/../output/build/rpi-firmware-5476720d52cf579dc1627715262b30ba1242525e/boot/bcm2711-rpi-4-b.dtb \
	${BINARIES_DIR}/rpi-firmware/bcm2711-rpi-4-b.dtb

# Convert and add splash screen for u-boot
/usr/bin/python3 ${BR2_EXTERNAL_BR2RAUC_PATH}/../uboot-splash/splash.py

# Add public key for SSH access, if it exists
if [ -e ${BR2_EXTERNAL_BR2RAUC_PATH}/../ssh-keys/id_ed25519.pub ]; then
	mkdir -p ${TARGET_DIR}/home/br2rauc/.ssh
	cp ${BR2_EXTERNAL_BR2RAUC_PATH}/../ssh-keys/id_ed25519.pub ${TARGET_DIR}/home/br2rauc/.ssh/authorized_keys
	chmod 600 ${TARGET_DIR}/home/br2rauc/.ssh/authorized_keys
fi

# Add a console on tty1
if [ -e ${TARGET_DIR}/etc/inittab ]; then
    grep -qE '^tty1::' ${TARGET_DIR}/etc/inittab || \
	sed -i '/GENERIC_SERIAL/a\
tty1::respawn:/sbin/getty -L  tty1 0 vt100 # HDMI console' ${TARGET_DIR}/etc/inittab
# systemd doesn't use /etc/inittab, enable getty.tty1.service instead
elif [ -d ${TARGET_DIR}/etc/systemd ]; then
    mkdir -p "${TARGET_DIR}/etc/systemd/system/getty.target.wants"
    ln -sf /lib/systemd/system/getty@.service \
       "${TARGET_DIR}/etc/systemd/system/getty.target.wants/getty@tty1.service"
fi

# enable systemd services
mkdir -p "${TARGET_DIR}/etc/systemd/system/multi-user.target.wants"
ln -sf /usr/lib/systemd/system/usb-gadget.service \
       "${TARGET_DIR}/etc/systemd/system/multi-user.target.wants/usb-gadget.service"

# Mount persistent data partitions
if [ -e ${TARGET_DIR}/etc/fstab ]; then
	# For configuration data
	# WARNING: data=journal is safest, but potentially slow!
	grep -qE 'LABEL=Data' ${TARGET_DIR}/etc/fstab || \
	echo "LABEL=Data /data ext4 defaults,data=journal,noatime 0 0" >> ${TARGET_DIR}/etc/fstab

	# For bulk data (eg: firmware updates)
	grep -qE 'LABEL=Upload' ${TARGET_DIR}/etc/fstab || \
	echo "LABEL=Upload /upload ext4 defaults,noatime 0 0" >> ${TARGET_DIR}/etc/fstab
fi

# Copy custom cmdline.txt file
if [ ${BOARD_NAME} = "raspberrypi5" ]; then
    install -D -m 0644 ${BR2_EXTERNAL_BR2RAUC_PATH}/board/raspberrypi/cmdline_5.txt ${BINARIES_DIR}/custom/cmdline.txt
else
    install -D -m 0644 ${BR2_EXTERNAL_BR2RAUC_PATH}/board/raspberrypi/cmdline.txt ${BINARIES_DIR}/custom/cmdline.txt
fi

# Copy RAUC certificate
if [ -e ${BR2_EXTERNAL_BR2RAUC_PATH}/openssl-ca/dev/ca.cert.pem ]; then
	install -D -m 0644 ${BR2_EXTERNAL_BR2RAUC_PATH}/openssl-ca/dev/ca.cert.pem ${TARGET_DIR}/etc/rauc/keyring.pem
else
	echo "RAUC CA certificate not found!"
	echo "...did you run the openssl-ca.sh script?"
	exit 1
fi

# Update RAUC compatible string
sed -i "/compatible/s/=.*\$/=${RAUC_COMPATIBLE}/" ${TARGET_DIR}/etc/rauc/system.conf

# Create rauc version file
echo "${RAUC_VERSION}" > ${TARGET_DIR}/etc/rauc/version
