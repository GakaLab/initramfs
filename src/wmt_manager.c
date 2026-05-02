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
#include <sys/wait.h>
#include <unistd.h>
/* ============================= */
/*         Structures            */
/* ============================= */
struct patch_payload
{
    uint32_t sequence_or_type;
    unsigned char address[4];
    char filename[NAME_MAX + 1];
};
struct patch_header
{
    char build_time[16];
    char platform[4];
    uint16_t hw_ver;
    uint16_t sw_ver;
    char info[4];
    char type[4];
};
static int set = 1;
/* ============================= */
/*           IOCTLs              */
/* ============================= */
#define SET_CHIP_ID _IOW(0x77, 1, int)
#define GET_CHIP_ID _IOR(0x77, 3, int)
#define EXT_CHIP_PWR_OFF _IOR(0x77, 7, int)
#define SDIO_AUTOK _IOR(0x77, 8, int)
#define MODULE_INIT _IOR(0x77, 4, int)
#define MODULE_CLEANUP _IOR(0x77, 5, int)
#define QUERY_CHIPID _IOR(0xa0, 22, int)
#define SET_STP_MODE _IOW(0xa0, 5, int)
#define CFG_NAME _IOWR(0xa0, 21, char*)
#define FUNC_ONOFF_CTRL _IOW(0xa0, 6, int)
#define LPBK_POWER_CTRL _IOW(0xa0, 7, int)
#define GET_CHIP_INFO _IOR(0xa0, 12, int)
#define SET_PATCH_NUM _IOW(0xa0, 14, int)
#define SET_PATCH_INFO _IOW(0xa0, 15, struct patch_payload*)
#define SET_ROM_PATCH_INFO _IOW(0xa0, 31, struct patch_payload*)
/* ============================= */
/*         Logging               */
/* ============================= */
static void klog_msg(const char* level, const char* fmt, ...)
{
    int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    if (fd < 0)
    {
        perror("open /dev/kmsg");
        return;
    }
    char body[384];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    char line[448];
    snprintf(line, sizeof(line), "[%s] %s\n", level, body);
    printf("%s\n", body);
    write(fd, line, strlen(line));
    close(fd);
}
/* ============================= */
/*      Device Node Helpers      */
/* ============================= */
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
/* ============================= */
/*      Kernel Module Loader     */
/* ============================= */
static int insert_module(const char* module_path)
{
    int fd = open(module_path, O_RDONLY);
    if (fd < 0)
    {
        klog_msg("err", "open %s failed", module_path);
        return 0;
    }
    int ret = syscall(SYS_finit_module, fd, "", 0);
    close(fd);
    if (ret != 0)
    {
        klog_msg("err", "load %s failed", module_path);
        return 0;
    }
    klog_msg("ok", "loaded %s", module_path);
    return 1;
}
/* ============================= */
/*        Patch Handling         */
/* ============================= */
static void register_patch(int fd, const char* filename)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/vendor/firmware/%s", filename);
    int file = open(path, O_RDONLY);
    if (file < 0)
    {
        klog_msg("err", "patch open failed: %s", path);
        return;
    }
    struct patch_header header;
    if (read(file, &header, sizeof(header)) != sizeof(header))
    {
        klog_msg("err", "patch read header failed");
        close(file);
        return;
    }

    if (strstr(filename, "WIFI_RAM_CODE"))
    {
        struct patch_payload payload;
        payload.sequence_or_type = 3;   // Try 1 or 3 based on your specific kernel driver
        memset(payload.address, 0, 4);  // Address is usually handled internally for RAM code
        strncpy(payload.filename, filename, NAME_MAX);
        payload.filename[sizeof(payload.filename) - 1] = '\0';
        if (ioctl(fd, SET_ROM_PATCH_INFO, &payload) == 0)
        {
            klog_msg("ok", "Manual WIFI_RAM_CODE registration sent");
        }
    }
    else
    {
        header.hw_ver = ntohs(header.hw_ver);
        header.sw_ver = ntohs(header.sw_ver);

        int fw_ver = ioctl(fd, GET_CHIP_INFO, 2);
        if ((header.hw_ver & 0xFF) != (fw_ver & 0xFF))
        {
            klog_msg("err", "patch version mismatch");
            close(file);
            return;
        }

        if (strstr(filename, "soc1_0_patch") != NULL)
        {
            struct patch_payload payload;
            uint8_t buffer = header.info[0];
            payload.sequence_or_type = (buffer & 0x0F);
            memcpy(payload.address, header.info, 4);
            payload.address[0] = 0;
            strncpy(payload.filename, filename, NAME_MAX);
            payload.filename[sizeof(payload.filename) - 1] = '\0';
            int patch_num = (buffer & 0xF0) >> 4;
            if (set != 0)
            {
                set = ioctl(fd, SET_PATCH_NUM, patch_num);
            }
            if (ioctl(fd, SET_PATCH_INFO, &payload) == 0)
                klog_msg("info", "num=%i seq=%i", patch_num, payload.sequence_or_type);
        }
        else
        {
            struct patch_payload payload;
            payload.sequence_or_type = header.type[3];
            memcpy(payload.address, header.info, 4);
            payload.address[0] = 0;
            strncpy(payload.filename, filename, NAME_MAX);
            payload.filename[sizeof(payload.filename) - 1] = '\0';
            ioctl(fd, SET_ROM_PATCH_INFO, &payload);
        }
    }
    close(file);
}
/* ============================= */
/*      Driver Load Sequence     */
/* ============================= */
static int load_connectivity_drivers()
{
    insert_module("/vendor/lib/modules/wmt_chrdev_wifi.ko");
    insert_module("/vendor/lib/modules/wlan_drv_gen4m.ko");
    make_node("/sys/class/wmtWifi/wmtWifi/dev", "/dev/wmtWifi", S_IFCHR | 0666);

    int wmtWifi = open("/dev/wmtWifi", O_WRONLY | O_NOCTTY);
    if (wmtWifi < 0)
    {
        klog_msg("err", "open /dev/wmtWifi failed");
        return -1;
    }

    int fd = open("/mnt/nvdata/APCFG/APRDEB/WIFI", O_RDONLY);
    char buffer[MAX_INPUT];
    size_t size = 0;
    if (fd < 0 || (size = read(fd, buffer, sizeof(buffer) - 1)) <= 0)
    {
        klog_msg("err", "open WIFI failed");
        return -1;
    }
    close(fd);
    ssize_t written = write(wmtWifi, buffer, size);
    klog_msg("info", "%zd bytes written to wmtWifi", written);
    close(wmtWifi);
    insert_module("/vendor/lib/modules/bt_drv.ko");
    insert_module("/vendor/lib/modules/met.ko");
    insert_module("/vendor/lib/modules/gps_drv.ko");
    wmtWifi = open("/dev/wmtWifi", O_WRONLY | O_NOCTTY);
    write(wmtWifi, "1", 1);
    close(wmtWifi);
    system("/sbin/mdev -s");
    system("/usr/bin/connect");
    klog_msg("info", "connectivity drivers loaded");
    return 1;
}
/* ============================= */
/*       Firmware Event Loop     */
/* ============================= */
static void firmware_event_loop()
{
    struct pollfd pfd;
    pfd.fd = open("/dev/stpwmt", O_RDWR | O_NOCTTY);
    if (pfd.fd < 0)
    {
        klog_msg("err", "open /dev/stpwmt failed");
        return;
    }
    pfd.events = POLLIN;
    char buffer[NAME_MAX];
    while (1)
    {
        int ret = poll(&pfd, 1, -1);
        if (ret <= 0)
        {
            klog_msg("err", "poll failed");
            continue;
        }
        if (pfd.revents & POLLIN)
        {
            int n = read(pfd.fd, buffer, sizeof(buffer) - 1);
            if (n <= 0) continue;
            buffer[n] = '\0';
            klog_msg("debug", "event: %s", buffer);
            /* ===================== */
            /* srh_patch — MCU load */
            /* ===================== */
            if (strcmp(buffer, "srh_patch") == 0)
            {
                int fd = open("/dev/stpwmt", O_RDWR | O_NOCTTY);
                if (pfd.fd < 0)
                {
                    klog_msg("err", "open /dev/stpwmt failed");
                    return;
                }
                register_patch(pfd.fd, "mtsoc1_0_patch_e1_hdr.bin");

                if (load_connectivity_drivers()) break;
            }
            /* ===================== */
            /* srh_rom_patch — RAM  */
            /* ===================== */
            else if (strcmp(buffer, "srh_rom_patch") == 0)
            {
                if (pfd.fd < 0)
                {
                    klog_msg("err", "open /dev/stpwmt failed");
                    return;
                }
                register_patch(pfd.fd, "soc1_0_ram_bt_1_1_hdr.bin");
                register_patch(pfd.fd, "soc1_0_ram_mcu_1_1_hdr.bin");
                register_patch(pfd.fd, "soc1_0_ram_wifi_1_1_hdr.bin");
                register_patch(pfd.fd, "WIFI_RAM_CODE_soc1_0_1_1.bin");
                ioctl(pfd.fd, LPBK_POWER_CTRL, 2);
            }
            else
                sleep(1);
        }
    }
    close(pfd.fd);
}
/* ============================= */
/*           Move Mount          */
/* ============================= */
static void safe_move(const char* old, const char* new)
{
    if (mount(old, new, NULL, MS_MOVE, NULL) < 0)
    {
        perror("mount --move");
    }
    else
    {
        printf("moved %s -> %s\n", old, new);
    }
}
/* ============================= */
/*            MAIN               */
/* ============================= */
int main()
{
    if (daemon(0, 0) != 0)
    {
        klog_msg("err", "daemon() failed");
        return -1;
    }
    klog_msg("info", "Starting WMT/STP setup");
    /* ------------------------- */
    /* Load core WMT driver      */
    /* ------------------------- */
    if (!insert_module("/vendor/lib/modules/wmt_drv.ko"))
    {
        klog_msg("err", "wmt_drv load failed");
        return -1;
    }
    /* ------------------------- */
    /* Create wmtdetect device   */
    /* ------------------------- */
    make_node("/sys/class/wmtdetect/wmtdetect/dev", "/dev/wmtdetect", S_IFCHR | 0666);
    int detect = open("/dev/wmtdetect", O_RDWR | O_NOCTTY);
    if (detect < 0)
    {
        klog_msg("err", "open wmtdetect failed");
        return -1;
    }
    /* ------------------------- */
    /* Chip Detection            */
    /* ------------------------- */
    int chipid = ioctl(detect, GET_CHIP_ID, 0);
    if (chipid < 0)
    {
        klog_msg("err", "GET_CHIP_ID failed");
        close(detect);
        return -1;
    }
    klog_msg("info", "chip detected: 0x%x", chipid);
    ioctl(detect, SDIO_AUTOK, chipid);
    ioctl(detect, EXT_CHIP_PWR_OFF);
    ioctl(detect, SET_CHIP_ID, chipid);
    /* ------------------------- */
    /* Cleanup + Init            */
    /* ------------------------- */
    if (ioctl(detect, MODULE_CLEANUP, chipid) != 0)
    {
        klog_msg("err", "MODULE_CLEANUP failed");
    }
    if (ioctl(detect, MODULE_INIT, chipid) != 0)
    {
        klog_msg("err", "MODULE_INIT failed");
        close(detect);
        return -1;
    }
    klog_msg("ok", "MODULE_INIT success");
    close(detect);

    /* ------------------------- */
    /* Calibrate NVRAM           */
    /* ------------------------- */

    /*int wifi = open("/mnt/nvdata/APCFG/APRDEB/WIFI", O_RDONLY);
    char buffer[2050];
    if (wifi < 0 || read(wifi, buffer, sizeof(buffer)) <= 0)
    {
        klog_msg("err", "open WIFI failed");
        return -1;
    }
    close(wifi);

    int nvram = open("/dev/nvram", O_WRONLY);
    if (nvram < 0)
    {
        klog_msg("err", "open Nvram failed");
        return -1;
    }

    if (write(nvram, buffer, sizeof(buffer)) != sizeof(buffer))
    {
        klog_msg("err", "write Nvram failed");
        close(nvram);
        return -1;
    }
    close(nvram);*/

    /* ------------------------- */
    /* Create STP Device         */
    /* ------------------------- */
    make_node("/sys/class/stpwmt/stpwmt/dev", "/dev/stpwmt", S_IFCHR | 0666);
    int stp = open("/dev/stpwmt", O_RDWR | O_NOCTTY);
    if (stp < 0)
    {
        klog_msg("err", "open /dev/stpwmt failed");
        return -1;
    }
    /* ------------------------- */
    /* Load WMT.cfg (FIXED PATH) */
    /* ------------------------- */
    char cfg_path[] = "/vendor/firmware/WMT.cfg";
    if (ioctl(stp, CFG_NAME, cfg_path) != 0)
    {
        klog_msg("err", "CFG_NAME failed");
    }
    else
    {
        klog_msg("ok", "CFG_NAME loaded");
    }
    /* ------------------------- */
    /* Query Chip                */
    /* ------------------------- */
    chipid = ioctl(stp, QUERY_CHIPID, 0);
    klog_msg("info", "STP chipid: 0x%x", chipid);
    /* ------------------------- */
    /* Configure STP Mode        */
    /* ------------------------- */
    int data = ((115200 & 0xFFFFFF) << 8) | ((2 & 0xF) << 4) | (3 & 0xF);
    ioctl(stp, SET_STP_MODE, data);
    klog_msg("ok", "SET_STP_MODE done");
    /* ------------------------- */
    ioctl(stp, FUNC_ONOFF_CTRL, 1);
    klog_msg("ok", "FUNC_ONOFF_CTRL enabled");
    ioctl(stp, LPBK_POWER_CTRL, 2);
    klog_msg("ok", "LPBK_POWER_CTRL issued");
    close(stp);
    firmware_event_loop();
    return 0;
}