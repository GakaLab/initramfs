#include "main.h"

int main(int argc, char *argv[])
{
    setup_rootfs();
    populate_devices();
    mount_rootfs();
    // list_files();
    //  show_variables(argc, argv);
    return sleep(0XFFFFFF);
}

static inline void setup_rootfs()
{
    mkdir("/proc", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    mkdir("/sys", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    mkdir("/dev", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, "");
    mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV, "");
    mount("tmpfs", "/dev", "tmpfs", MS_STRICTATIME, "uid=0,gid=0,mode=755");
    mkdir("/dev/block", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

static inline void populate_devices()
{
    mknod("/dev/null", S_IFCHR | DEFFILEMODE, MKDEV(1, 3));
    mknod("/dev/port", S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, MKDEV(1, 4));
    mknod("/dev/zero", S_IFCHR | DEFFILEMODE, MKDEV(1, 5));
    mknod("/dev/full", S_IFCHR | DEFFILEMODE, MKDEV(1, 7));
    mknod("/dev/random", S_IFCHR | DEFFILEMODE, MKDEV(1, 8));
    mknod("/dev/urandom", S_IFCHR | DEFFILEMODE, MKDEV(1, 9));
    mknod("/dev/kmsg", S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, MKDEV(1, 11));
    mknod("/dev/tty", S_IFCHR | DEFFILEMODE, MKDEV(5, 0));
    mknod("/dev/console", S_IFCHR | S_IRUSR | S_IWUSR | S_IWGRP, MKDEV(5, 1));
    mknod("/dev/ptmx", S_IFCHR | DEFFILEMODE, MKDEV(5, 2));
    mknod("/dev/fb0", S_IFCHR | DEFFILEMODE, MKDEV(29, 0));
    mknod("/dev/block/mmcblk0", S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, MKDEV(179, 1));
    mknod("/dev/block/mmcblk0p31", S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, MKDEV(179, 31));
    mknod("/dev/block/mmcblk0p32", S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, MKDEV(179, 32));
}

static inline void list_files(char *path)
{
    DIR *directory;
    struct dirent *folder;
    directory = opendir(path);
    if (directory)
    {
        while ((folder = readdir(directory)) != NULL)
        {
            klog(folder->d_name);
        }
        closedir(directory);
    }
}

static inline void klog(char *message)
{
    int size = strlen(message) + 17;
    char buffer[size];
    sprintf(buffer, "DEBUG MESSAGE: %s", message);
    printf("%s\n", buffer);
    int kmsg = open("/dev/kmsg", O_WRONLY);
    if (write(kmsg, buffer, size) == -1)
        show_error();
    close(kmsg);
}

static inline void show_error()
{
    char message[18 + strlen(strerror(errno))];
    sprintf(message, "DEBUG MESSAGE: %s\n", strerror(errno));
    printf("%s\n", message);
    klog(strerror(errno));
}

static inline void print_file(char *path)
{
    FILE *mounts = fopen(path, "r");
    char buffer[128];
    klog(path);
    while (fgets(buffer, sizeof(buffer), mounts) != NULL)
    {
        klog(buffer);
    }
}

static inline void mount_rootfs()
{
    mkdir("/mnt", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (mount(rootfs, "/mnt", "ext4", MS_RELATIME | MS_STRICTATIME, "") == 0)
    {
        klog("Mount Successful");
        print_file("/proc/mounts");
        FILE *log = fopen("/mnt/log.txt", "w");
        list_files("/mnt");
        fprintf(log, "Some text");
        fclose(log);
        reboot_device(RB_KEXEC);
    }
    else
        show_error();
}

static inline void show_variables(int count, char *argument[])
{
    FILE *file = fopen("/proc/cmdline", "r");
    char buffer[get_file_size(file)];
    if (fread(buffer, sizeof *buffer, 2048, file) > 0)
    {
        klog("Parameters");
        char *parameter = strtok(buffer, " ");
        while (parameter != NULL && parameter != "")
        {
            char buffer[strlen(parameter)];
            sprintf(buffer, "%s", parameter);
            // set_reason(buffer);
            parameter = strtok(NULL, " ");
        }
    }
    fclose(file);

    klog("Init Arguments");
    for (int i = 0; i < count; i++)
    {
        // klog(argument[i]);
    }

    klog("Environment Variables");
    for (int i = 0; environ[i] != NULL; i++)
    {
        // klog(environ[i]);
    }
}

static inline int get_file_size(FILE *file)
{
    int count = 0;
    for (char c = getc(file); getc(file) != EOF; count++)
        ;
    rewind(file);
    return count - 1;
}

static inline void reboot_device(int command)
{
    if (mountpoint("/mnt"))
        umount("/mnt");
    sync();
    switch (command)
    {
    case RB_POWER_OFF:
        syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                LINUX_REBOOT_CMD_POWER_OFF, NULL);
        break;
    case RB_KEXEC:
        syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                LINUX_REBOOT_CMD_RESTART2, "bootloader");
        break;
    default:
        syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                LINUX_REBOOT_CMD_RESTART2, "PMIC_cold_reboot");
        break;
    }
}

static inline void set_reason(char *parameter)
{
    if ((strstr(parameter, "androidboot.bootreason")) != NULL)
    {
        char *pointer;
        char *token = strtok(parameter, "=");
        while (token != NULL)
        {
            if (strcmp(token, "reboot") == 0)
                reboot_device(RB_AUTOBOOT);
            else if (strcmp(token, "PMIC_cold_reboot") == 0)
                bootloader = true;
            else if (strcmp(token, "reboot_longkey") == 0)
                reboot_device(RB_AUTOBOOT);
            else if (strcmp(token, "androidboot.bootreason") != 0)
            {
                klog(token);
                reboot_device(RB_AUTOBOOT);
            }
            token = strtok(NULL, " ");
        }
    }
}

static bool mountpoint(char *path)
{
    struct stat parent;
    struct stat child;
    char buffer[strlen(path) + 4];
    sprintf(buffer, "%s/..", path);
    stat(buffer, &parent);
    stat(path, &child);
    return parent.st_dev == child.st_dev;
}