#ifndef MAIN_H
#define MAIN_H

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <linux/kdev_t.h>
#include <linux/limits.h>
#include <linux/reboot.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>

extern char **environ;

bool bootloader = false;

static inline void setup_rootfs();
static inline void populate_devices();
static inline void list_files(char *path);
static inline void klog(char *message);
static inline void show_error(char *target);
static inline void show_variables(int count, char *argument[]);
static inline void mount_rootfs();
static inline void print_file(char *path);
static bool mountpoint(char *path);
static inline void reboot_device(int command);
static inline void set_reason(char *parameter);
static inline int get_file_size(FILE *file);
const char *rootfs = "/dev/block/mmcblk0p31";
#endif