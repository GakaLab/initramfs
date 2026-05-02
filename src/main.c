#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <linux/reboot.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <unistd.h>

pthread_t thread;

static void panic()
{
    syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2,
            "bootloader");
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

static int get_udc(const char* path, char* out, size_t out_len)
{
    DIR* d = opendir(path);

    if (!d) return 0;

    struct dirent* entry;

    while ((entry = readdir(d)) != NULL)
    {
        if (entry->d_name[0] == '.') continue;

        snprintf(out, out_len, "%s", entry->d_name);

        closedir(d);
        return 1;
    }

    closedir(d);
    return 0;
}

static void* wmt_thread(void* arg)
{
    system("/bin/wmt_manager");
    return NULL;
}

int main(void)
{
    if (mkdir("/proc", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }

    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, "") < 0)
    {
        panic();
    }
    if (mkdir("/sys", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }

    if (mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV, "") < 0)
    {
        panic();
    }
    if (mkdir("/dev", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }
    if (mount("dev", "/dev", "tmpfs", MS_RELATIME, "mode=755") < 0)
    {
        panic();
    }

    mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));
    mknod("/dev/zero", S_IFCHR | 0666, makedev(1, 5));
    mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11));
    mknod("/dev/console", S_IFCHR | 0600, makedev(5, 1));
    mknod("/dev/tty", S_IFCHR | 0666, makedev(5, 0));
    mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8));
    mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9));

    if (mkdir("/dev/pts", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }
    if (mount("devpts", "/dev/pts", "devpts", MS_RELATIME,
              "newinstance,ptmxmode=0666,mode=620,gid=5") < 0)
    {
        panic();
    }

    symlink("/dev/pts/ptmx", "/dev/ptmx");

    if (mkdir("/dev/block", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }

    make_node("/sys/block/mmcblk0/mmcblk0p28/dev", "/dev/block/boot", S_IFBLK | 0600);
    make_node("/sys/block/mmcblk0/mmcblk0p36/dev", "/dev/block/super", S_IFBLK | 0600);
    make_node("/sys/block/mmcblk0/mmcblk0p39/dev", "/dev/block/userdata", S_IFBLK | 0600);

    if (mkdir("/home", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }

    if (mount("/dev/block/userdata", "/home", "f2fs", MS_RELATIME, "") < 0)
    {
        panic();
    }

    if (mkdir("/usr", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }

    if (mount("/dev/block/super", "/usr", "ext4", MS_RELATIME, "") < 0)
    {
        panic();
    }

    if (mkdir("/rootfs", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }

    if (mount("/dev/block/boot", "/rootfs", "ext4", MS_RELATIME, "") < 0)
    {
        panic();
    }

    sethostname("tecno", 6);

    if (mkdir("/sys/kernel/config", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }

    if (mount("configfs", "/sys/kernel/config", "configfs", MS_NOSUID | MS_NOEXEC | MS_NODEV, "") <
        0)
    {
        panic();
    }

    if (mkdir("/sys/kernel/config/usb_gadget/g1", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }

    FILE* fp;

    fp = fopen("/sys/kernel/config/usb_gadget/g1/idVendor", "w");

    if (fp)
    {
        fprintf(fp, "0x0e8d");
        fclose(fp);
    }

    fp = fopen("/sys/kernel/config/usb_gadget/g1/idProduct", "w");

    if (fp)
    {
        fprintf(fp, "0x201c");
        fclose(fp);
    }

    if (mkdir("/sys/kernel/config/usb_gadget/g1/strings/0x409", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }

    // Good
    fp = fopen("/sys/kernel/config/usb_gadget/g1/strings/0x409/manufacturer", "w");

    if (fp)
    {
        fprintf(fp, "MediaTek");
        fclose(fp);
    }

    fp = fopen("/sys/kernel/config/usb_gadget/g1/strings/0x409/product", "w");

    if (fp)
    {
        fprintf(fp, "MTK Debug Console");
        fclose(fp);
    }

    if (mkdir("/sys/kernel/config/usb_gadget/g1/configs/b.1", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }

    if (mkdir("/sys/kernel/config/usb_gadget/g1/functions/acm.usb0", 0755) < 0 && errno != EEXIST)
    {
        panic();
    }

    symlink("/sys/kernel/config/usb_gadget/g1/functions/acm.usb0",
            "/sys/kernel/config/usb_gadget/g1/configs/b.1/f1");

    char udc[256];

    if (get_udc("/sys/class/udc", udc, sizeof(udc)))
    {
        fp = fopen("/sys/kernel/config/usb_gadget/g1/UDC", "w");

        if (fp)
        {
            fprintf(fp, "%s", udc);
            fclose(fp);
        }
    }

    make_node("/sys/class/tty/ttyGS0/dev", "/dev/ttyGS0", S_IFCHR | 0600);

    // pthread_create(&thread, NULL, wmt_thread, NULL);
    system("/bin/wmt_manager");
    while (1)
    {
        pid_t pid = fork();

        if (pid == 0)
        {
            execl("/bin/busybox", "busybox", "getty", "-l", "/bin/login", "-L", "115200",
                  "/dev/ttyGS0", "vt100", NULL);
            exit(127);
        }
        else if (pid > 0)
        {
            int status;
            waitpid(pid, &status, 0);
        }
        else
            panic();
    }

    return 0;
}