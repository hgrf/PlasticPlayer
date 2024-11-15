#!/bin/sh
modprobe libcomposite
mkdir -p /sys/kernel/config/usb_gadget/mygadget
cd /sys/kernel/config/usb_gadget/mygadget
echo 0x1d6b > idVendor
echo 0x0104 > idProduct
echo 0x0100 > bcdDevice
echo 0x0200 > bcdUSB
mkdir -p strings/0x409
echo "1234567890" > strings/0x409/serialnumber
echo "hgrf" > strings/0x409/manufacturer
echo "PlasticPlayer3" > strings/0x409/product
mkdir -p configs/c.1/strings/0x409
echo "Config 1: ECM network" > configs/c.1/strings/0x409/configuration
mkdir -p functions/ecm.usb0
echo "12:22:33:44:55:66" > functions/ecm.usb0/host_addr
echo "16:22:33:44:55:66" > functions/ecm.usb0/dev_addr
ln -s functions/ecm.usb0 configs/c.1/
ls /sys/class/udc > UDC
