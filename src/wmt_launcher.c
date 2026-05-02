#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>

#define LOG_TAG "wmt_launcher"
#define WMT_DEV_NODE "/dev/stpwmt"

/* IOCTL Commands */
#define WMT_IOCTL_GET_CHIPID          0x8004A00C
#define WMT_IOCTL_SET_CHIPID          0x4004A017
#define WMT_IOCTL_SET_PATCH_NUM       0x4004A00E
#define WMT_IOCTL_SET_PATCH_INFO      0x4008A00F
#define WMT_IOCTL_LPBK_POWER_CTRL     0x4004A007
#define WMT_IOCTL_SET_STP_MODE        0x4004A005
#define WMT_IOCTL_EXIT                0x4004A00D
#define WMT_IOCTL_GET_DRV_CHIPID      0x8004A016
#define WMT_IOCTL_SET_ROM_PATCH_INFO  0x4008A01F

static int g_wmt_fd = -1;
static int g_chip_id = -1;
static int g_stop_launcher = 0;

void *power_on_thread(void *arg);
int handle_command(char *cmd);
int cmd_hdr_sch_patch(unsigned int chip_id);
int cmd_hdr_sch_rom_patch(unsigned int chip_id);
void launcher_set_prop(const char *key, const char *val);

void sig_handler(int sig) {
    printf("[%s] Received signal %d, exiting...\n", LOG_TAG, sig);
    g_stop_launcher = 1;
    if (g_wmt_fd >= 0) {
        ioctl(g_wmt_fd, WMT_IOCTL_EXIT, 1);
    }
}

int main(int argc, char **argv) {
    char prop_buf[256];
    int ret;

    while (1) {
        char *driver_ready = getenv("VENDOR_CONNSYS_DRIVER_READY");
        if (driver_ready && strcmp(driver_ready, "yes") == 0) {
            break;
        }
        usleep(300000);
    }

    g_wmt_fd = open(WMT_DEV_NODE, O_RDWR);
    while (g_wmt_fd < 0) {
        printf("[%s] Cannot open %s: %s\n", LOG_TAG, WMT_DEV_NODE, strerror(errno));
        usleep(300000);
        g_wmt_fd = open(WMT_DEV_NODE, O_RDWR);
    }

    char *chip_env = getenv("PERSIST_VENDOR_CONNSYS_CHIPID");
    if (chip_env) {
        g_chip_id = (int)strtoul(chip_env, NULL, 16);
    }
    
    if (g_chip_id <= 0) {
        g_chip_id = ioctl(g_wmt_fd, WMT_IOCTL_GET_DRV_CHIPID, 0);
    }

    printf("[%s] Launcher starting for ChipID: 0x%04x\n", LOG_TAG, g_chip_id);

    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    pthread_t pwr_thread;
    if (pthread_create(&pwr_thread, NULL, power_on_thread, &g_chip_id) != 0) {
        printf("[%s] Failed to create power thread\n", LOG_TAG);
    } else {
        pthread_detach(pwr_thread);
    }

    struct pollfd fds[1];
    fds[0].fd = g_wmt_fd;
    fds[0].events = POLLIN;

    char cmd_buf[256];
    while (!g_stop_launcher) {
        ret = poll(fds, 1, -1);
        if (ret > 0 && (fds[0].revents & POLLIN)) {
            memset(cmd_buf, 0, sizeof(cmd_buf));
            ssize_t bytes = read(g_wmt_fd, cmd_buf, sizeof(cmd_buf) - 1);
            if (bytes > 0) {
                handle_command(cmd_buf);
            }
        } else if (ret < 0 && errno != EINTR) {
            break;
        }
    }

    if (g_wmt_fd >= 0) close(g_wmt_fd);
    return 0;
}

void *power_on_thread(void *arg) {
    int cid = *(int *)arg;
    int retry = 20;
    
    pthread_setname_np(pthread_self(), "pwr_on_conn");
    
    while (retry-- > 0) {
        if (ioctl(g_wmt_fd, WMT_IOCTL_LPBK_POWER_CTRL, 1) == 0) {
            printf("[%s] Power on successful for 0x%x\n", LOG_TAG, cid);
            return NULL;
        }
        ioctl(g_wmt_fd, WMT_IOCTL_LPBK_POWER_CTRL, 0);
        usleep(1000000);
    }
    printf("[%s] Power on failed after retries\n", LOG_TAG);
    return NULL;
}

int handle_command(char *cmd) {
    printf("[%s] Command received: %s\n", LOG_TAG, cmd);
    
    if (strstr(cmd, "srh_patch")) {
        cmd_hdr_sch_patch(g_chip_id);
    } else if (strstr(cmd, "srh_rom_patch")) {
        cmd_hdr_sch_rom_patch(g_chip_id);
    } else {
        char resp[256] = "cmd not found";
        write(g_wmt_fd, resp, strlen(resp));
        return -1;
    }
    
    char resp[256] = "ok";
    write(g_wmt_fd, resp, strlen(resp));
    return 0;
}

int cmd_hdr_sch_patch(unsigned int chip_id) {
    DIR *dir;
    struct dirent *ent;
    char patch_prefix[32];
    char search_path[256] = "/vendor/firmware";

    snprintf(patch_prefix, sizeof(patch_prefix), "mt%04x", chip_id);
    printf("[%s] Searching patch files with prefix: %s\n", LOG_TAG, patch_prefix);

    dir = opendir(search_path);
    if (dir == NULL) {
        printf("[%s] Failed to open patch directory\n", LOG_TAG);
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, patch_prefix, strlen(patch_prefix)) == 0) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", search_path, ent->d_name);
            
            int fd = open(full_path, O_RDONLY);
            if (fd >= 0) {
                char ver_buf[16];
                if (read(fd, ver_buf, sizeof(ver_buf)) > 0) {
                    printf("[%s] Patch opened: %s, Version: %c%c%c%c\n", LOG_TAG, ent->d_name, 
                           ver_buf[0], ver_buf[1], ver_buf[2], ver_buf[3]);
                    
                    launcher_set_prop("persist.vendor.connsys.patch.version", ver_buf);
                    
                    ioctl(g_wmt_fd, WMT_IOCTL_SET_PATCH_NUM, 1);
                    ioctl(g_wmt_fd, WMT_IOCTL_SET_PATCH_INFO, full_path);
                }
                close(fd);
            }
        }
    }
    closedir(dir);
    return 0;
}

int cmd_hdr_sch_rom_patch(unsigned int chip_id) {
    DIR *dir;
    struct dirent *ent;
    char search_path[256] = "/vendor/firmware";
    
    printf("[%s] Searching ROM patches for Chip ID: mt%04x\n", LOG_TAG, chip_id);

    dir = opendir(search_path);
    if (dir == NULL) return -1;

    while ((ent = readdir(dir)) != NULL) {
        if (strstr(ent->d_name, "patch") != NULL) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", search_path, ent->d_name);
            
            printf("[%s] ROM Patch found: %s\n", LOG_TAG, ent->d_name);
            
            // Send ROM patch information to the kernel
            ioctl(g_wmt_fd, WMT_IOCTL_SET_ROM_PATCH_INFO, full_path);
        }
    }
    closedir(dir);
    return 0;
}

void launcher_set_prop(const char *key, const char *val) {
    setenv(key, val, 1);
}