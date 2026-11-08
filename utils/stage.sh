#!/bin/sh
# Stage an image into tranfs for kaeru's "Flash staged image" / oem flash-staged path.
#
# On the phone, in Termux (with root) or any rooted shell:
#     sh utils/stage.sh /sdcard/boot.img
#
# From a PC with adb + root (the same script over the wire):
#     adb push utils/stage.sh /data/local/tmp/
#     adb push boot.img /data/local/tmp/
#     adb shell su -c 'sh /data/local/tmp/stage.sh /data/local/tmp/boot.img'
#
# A host that can reach fastboot needs none of this - write the partition directly:
#     fastboot flash <partition> <image>
#
# The store shares tranfs with the boot backup, so staging destroys the backup.
set -e

IMG=$1
[ -n "$IMG" ] || { echo "usage: $0 <image>" >&2; exit 2; }
[ -f "$IMG" ] || { echo "$IMG: no such file" >&2; exit 2; }

T=/dev/block/by-name/tranfs
[ -b "$T" ] || { echo "$T: not here - run this on the phone, or via 'adb shell su -c'" >&2; exit 2; }
[ "$(id -u)" = 0 ] || { echo "need root (Termux: su/tsu; PC: adb shell su -c)" >&2; exit 2; }

set -- $(cksum "$IMG")
printf 'KST1 %s %s %s\n' "$2" "$1" "${IMG##*/}" |
    dd of="$T" bs=4096 seek=$((0xC800000 / 4096)) conv=notrunc
dd if="$IMG" of="$T" bs=4096 seek=$((0xC801000 / 4096)) conv=fsync

echo "staged $IMG ($2 bytes, cksum $1) in tranfs"
echo "flash it from the menu (Flash staged image) or: fastboot oem flash-staged <partition>"
