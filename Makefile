CC := aarch64-linux-gnu-gcc
CFLAGS := -mtune=cortex-a55 -march=armv8.2-a -pipe -O2 -fpic -fpie -std=gnu17 
OUTPUT := initrd


.PHONY: init ramdisk boot.img test

all: boot.img

init:
	clear
	[ -d $@rd ] || mkdir $@rd
	${CC} ${CFLAGS} -static main.c -o ${OUTPUT}/$@
	chmod 777 ${OUTPUT}/$@

ramdisk: init
	cd initrd && echo init | cpio -o --format=newc -R root:root | gzip -9 > ../ramdisk.cpio.gz

boot.img: ramdisk
	mkbootimg --header_version 2 \
        --os_version 13.0.0 --os_patch_level 2023-01 \
		--kernel Image.gz --ramdisk ramdisk.cpio.gz --dtb dtb \
		--pagesize 0x00000800 --base 0x00000000 \
		--kernel_offset 0x40080000 --ramdisk_offset 0x47c80000 \
		--second_offset 0x00000000 --tags_offset 0x4bc80000 \
		--dtb_offset 0x000000004bc80000 --board CY-KI7-V7510 \
		--cmdline 'bootopt=64S3,32N2,64N2 loglevel=7 printk.devkmsg=on \
		    androidboot.selinux=permissive buildvariant=eng' -o $@

flash:
	adb -d wait-for-usb-device reboot bootloader
	fastboot flash boot_a boot.img
	fastboot reboot
	sleep 20
	fastboot flash boot_a stock.img
	fastboot continue
	adb -d wait-for-usb-device shell su -c cat /sys/fs/pstore/console-ramoops-0 > console

metamode:
	mtk payload --metamode FASTBOOT

test: init
	adb push initrd/init /data/local/tmp/init
	adb shell su -c mv /data/local/tmp/init /data/local/tmp/root/init
	adb shell su -c chroot /data/local/tmp/root /init