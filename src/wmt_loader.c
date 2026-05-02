#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <linux/mount.h>
#include <linux/reboot.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define IOCTL_WMT_SET_CHIPID _IOW(0x77, 1, unsigned int)
#define IOCTL_WMT_GET_SOC_CHIPID _IOR(0x77, 3, int)
#define IOCTL_WMT_MOD_INIT _IOR(0x77, 4, unsigned int)
#define IOCTL_WMT_MOD_CLEANUP _IOR(0x77, 5, unsigned int)
#define IOCTL_WMT_POWER_ON _IOR(0x77, 6, 0)
#define IOCTL_WMT_AUTOK _IOR(0x77, 8, unsigned int)

typedef struct
{
    unsigned int chip_id;
    unsigned int module_id;
    const char* name;
} ChipModuleMap;

static const ChipModuleMap chip_map[] = {
    {801, 26421, "Everest"},    {821, 26421, "Everest"},         {823, 26421, "Everest"},
    {806, 26453, "Jade"},       {633, 26519, "SoC1.0"},          {1287, 26457, "SoC1.0-E1"},
    {1361, 26455, "SoC1.0-E2"}, {1680, 26467, "SoC2.0-A"},       {1811, 26485, "SoC2.0-B"},
    {1928, 26481, "SoC2.0-C"},  {0x6765, 26469, "Helio P23/P35"}};

static unsigned int map_chip_to_module(unsigned int chip_id, const char** name_out)
{
    for (size_t i = 0; i < sizeof(chip_map) / sizeof(chip_map[0]); i++)
    {
        if (chip_map[i].chip_id == chip_id)
        {
            if (name_out) *name_out = chip_map[i].name;
            return chip_map[i].module_id;
        }
    }
    if (name_out) *name_out = "Unknown";
    return chip_id;
}

static int insert_module(const char* module_path)
{
    int fd = open(module_path, O_RDONLY);
    if (fd < 0)
    {
        return 0;
    }
    int ret = syscall(SYS_finit_module, fd, "", 0);
    close(fd);
    if (ret != 0)
    {
        return 0;
    }
    return 1;
}

static void make_node(const char* sys_dev_path, const char* node_path, __mode_t mode)
{
    FILE* fp = fopen(sys_dev_path, "r");
    if (!fp) return;
    int major, minor;
    if (fscanf(fp, "%d:%d", &major, &minor) != 2)
    {
        fclose(fp);
        return;
    }
    fclose(fp);
    mknod(node_path, mode, makedev(major, minor));
}

int main(int argc, const char** argv)
{
    if (!insert_module("/vendor/lib/modules/wmt_drv.ko"))
    {
        return -1;
    }
    make_node("/sys/class/wmtdetect/wmtdetect/dev", "/dev/wmtdetect", S_IFCHR | 0666);

    int fd = open("/dev/wmtdetect", O_RDWR);
    if (fd < 0)
    {
        return -1;
    }

    if (ioctl(fd, IOCTL_WMT_POWER_ON) != -1)
    {
        close(fd);
        return -1;
    }

    int chip_id = ioctl(fd, IOCTL_WMT_GET_SOC_CHIPID);
    if (chip_id <= 0)
    {
        close(fd);
        return -1;
    }

    const char* chip_name;
    unsigned int module_id = map_chip_to_module(chip_id, &chip_name);

    printf("chipid (0x%x) detected → %s (module %u)\n", chip_id, chip_name, module_id);

    if (ioctl(fd, IOCTL_WMT_SET_CHIPID, module_id) != 0)
    {
        return -1;
    }
    if (ioctl(fd, IOCTL_WMT_AUTOK, module_id) != 0)
    {
        return -1;
    }
    if (ioctl(fd, IOCTL_WMT_MOD_CLEANUP, module_id) != 0)
    {
        return -1;
    }
    if (ioctl(fd, IOCTL_WMT_MOD_INIT, module_id) != 0)
    {
        return -1;
    }
    return 0;
}
