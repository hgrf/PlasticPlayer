from https://lore.kernel.org/buildroot/20241230000816.34cc40be@windsurf/T/#u
(should soon be part of buildroot release)

sudo mount /dev/mmcblk0p1 /boot
sudo EDITOR=vi rpi-eeprom-config -e

```
[all]
BOOT_UART=0
WAKE_ON_GPIO=1
POWER_OFF_ON_HALT=0
```

NOTE: POWER_OFF_ON_HALT=1 only makes sense if WAKE_ON_GPIO=0, c.f. https://www.raspberrypi.com/documentation/computers/raspberry-pi.html

=> so for now we have no reason to change the firmware params (or the firmware itself for that matter)