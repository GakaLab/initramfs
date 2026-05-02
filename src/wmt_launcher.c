#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LOG_TAG "wmt_launcher"
#define WMT_DEV_NODE "/dev/stpwmt"
#define CFG_FILE_PATH "/vendor/firmware/WMT.cfg"

/* IOCTL Commands extracted from decompilation */
#define WMT_IOCTL_GET_CHIPID 0x8004A00C
#define WMT_IOCTL_SET_CHIPID 0x4004A017
#define WMT_IOCTL_SET_PATCH_NUM 0x4004A00E
#define WMT_IOCTL_SET_PATCH_INFO 0x4008A00F
#define WMT_IOCTL_LPBK_POWER_CTRL 0x4004A007
#define WMT_IOCTL_SET_STP_MODE 0x4004A005
#define WMT_IOCTL_EXIT 0x4004A00D
#define WMT_IOCTL_GET_DRV_CHIPID 0x8004A016
#define WMT_IOCTL_SET_ROM_PATCH_INFO 0x4008A01F

/* Global State */
static int g_wmt_fd = -1;
static int g_chip_id = -1;
static int g_stop_launcher = 0;

/* Function Prototypes */
void* power_on_thread(void* arg);
int load_patches(const char* path);
int handle_command(char* cmd);

void sig_handler(int sig)
{
    g_stop_launcher = 1;
    if (g_wmt_fd >= 0)
    {
        ioctl(g_wmt_fd, WMT_IOCTL_EXIT, 1);
    }
}


void* power_on_thread(void* arg)
{
    int cid = *(int*)arg;
    int retry = 20;

    pthread_setname_np(pthread_self(), "pwr_on_conn");

    while (retry-- > 0)
    {
        if (ioctl(g_wmt_fd, WMT_IOCTL_LPBK_POWER_CTRL, 1) == 0)
        {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Power on successful for 0x%x", cid);
            return NULL;
        }
        ioctl(g_wmt_fd, WMT_IOCTL_LPBK_POWER_CTRL, 0);  // Reset
        usleep(1000000);                                // 1s
    }
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Power on failed after retries");
    return NULL;
}

int handle_command(char* cmd)
{
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "Command received: %s", cmd);

    char resp[256] = "ok";
    if (strstr(cmd, "srh_patch"))
    {
        // Implement specific patch searching logic based on Chip ID
        // This usually involves opendir("/vendor/firmware") and string matching
        snprintf(resp, sizeof(resp), "ok");
    }
    else
    {
        snprintf(resp, sizeof(resp), "cmd not found");
    }

    write(g_wmt_fd, resp, strlen(resp));
    return 0;
}

int main(int argc, char** argv)
{
    int stpwmt = open(/dev/stpwmt, O_RDWR);
    int chipid = ioctl(stpwmt, WMT_IOCTL_GET_DRV_CHIPID, 0);
    }

    printf("Launcher starting for ChipID: 0x%04x", chipid);

    /* 4. Setup Signal Handlers */
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* 5. Start Power-On Thread */
    pthread_t pwr_thread;
    if (pthread_create(&pwr_thread, NULL, power_on_thread, &chipid) != 0)
    {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to create power thread");
    }
    else
    {
        pthread_detach(pwr_thread);
    }

    /* 6. Main Loop: Handle Patching and Control */
    struct pollfd fds[1];
    fds[0].fd = g_wmt_fd;
    fds[0].events = POLLIN;

    char cmd_buf[256];
    while (!g_stop_launcher)
    {
        ret = poll(fds, 1, -1);  // Wait indefinitely
        if (ret > 0 && (fds[0].revents & POLLIN))
        {
            memset(cmd_buf, 0, sizeof(cmd_buf));
            ssize_t bytes = read(g_wmt_fd, cmd_buf, sizeof(cmd_buf) - 1);
            if (bytes > 0)
            {
                handle_command(cmd_buf);
            }
        }
        else if (ret < 0 && errno != EINTR)
        {
            break;
        }
    }

    if (g_wmt_fd >= 0) close(g_wmt_fd);
    return 0;
}
