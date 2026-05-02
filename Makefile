TARGET	:= 0e8d:201c
DEVICE	:= /dev/ttyACM0
CMDLINE := 'root=/dev/mmcblk0p28 init=/sbin/init'

.PHONY: busybox.img

all: boot.img

init: src/main.c
	aarch64-linux-gnu-gcc -static -o initrd/$@ $<
	aarch64-linux-gnu-strip --strip-unneeded initrd/$@

login: src/login.c
	aarch64-linux-gnu-gcc -static -o initrd/bin/$@ $<
	aarch64-linux-gnu-strip --strip-unneeded initrd/bin/$@

wmt_manager: src/wmt_manager.c
	aarch64-linux-gnu-gcc -static -o initrd/bin/$@ $<
	aarch64-linux-gnu-strip --strip-unneeded initrd/bin/$@

wmt_loader: src/wmt_loader.c
	aarch64-linux-gnu-gcc -o initrd/bin/$@ $<
	aarch64-linux-gnu-strip --strip-unneeded initrd/bin/$@

rescue: src/rescue.c
	aarch64-linux-gnu-gcc -static -o initrd/sbin/$@ $<
	aarch64-linux-gnu-strip --strip-unneeded initrd/sbin/$@

ramdisk: login rescue wmt_manager wmt_loader
	python script.py -r -i initrd -o ramdisk.cpio.gz

#boot.img: ramdisk
#	mkbootimg --os_patch_level 2022-01 \
#	   --header_version 2 --os_version 10.0.0 \
#	   --kernel Image.gz --ramdisk ramdisk.cpio.gz \
#	   --recovery_dtbo dtbo.img --dtb mt6765.dtb \
#	   --pagesize 0x800 --base 0x40000000 --kernel_offset 0x80000 \
#	   --ramdisk_offset 0x11B00000 --second_offset 0xf00000 \
#	   --tags_offset 0x7880000 --dtb_offset 0x7880000 \
#	   --cmdline 'bootopt=64S3,32N2,64N2 buildvariant=user' \
#	   --board CY-KD7-H6211-F --output boot.img

boot.img: ramdisk
	mkbootimg --header_version 2 \
		--os_version 10.0.0 \
		--os_patch_level 2022-01 \
		--kernel Image.gz \
		--ramdisk ramdisk.cpio.gz \
		--dtb mt6765.dtb \
		--pagesize 0x800 \
		--base 0x40000000 \
		--kernel_offset 0x80000 \
		--ramdisk_offset 0x11B00000 \
		--tags_offset 0x7880000 \
		--dtb_offset 0x7880000 \
		--board CY-KD7-H6211-F \
		--cmdline 'bootopt=64S3,32N2,64N2 buildvariant=user' \
		-o boot.img

flash: boot.img
	@if adb get-state 1>/dev/null 2>&1; then \
	   echo "rescueing to bootloader..."; \
	   adb rescue bootloader; \
	fi
	fastboot flash recovery boot.img
	fastboot reboot recovery
	while true; do \
	    if lsusb | grep -q "$(TARGET)" && [ -e "$(DEVICE)" ]; then break; fi; \
	    if lsusb | grep -q "$(TARGET)" && [ -e "/dev/android_fastboot" ]; then exit; fi; \
	    sleep 1; \
	done
	stty rows 34 cols 162 < "$(DEVICE)"
	screen "$(DEVICE)" 115200

shell:
	stty rows 34 cols 162 < "$(DEVICE)"
	screen "$(DEVICE)" 115200
