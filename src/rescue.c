#include <linux/reboot.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    sync();
    system("/bin/umount -al");
    if (argc > 1)
    {
        if (strcmp(argv[1], "-p") == 0)
        {
            syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                    LINUX_REBOOT_CMD_POWER_OFF, NULL);
        }
        else if (strcmp(argv[1], "bootloader") == 0)
        {
            syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                    LINUX_REBOOT_CMD_RESTART2, "bootloader");
        }
        else if (strcmp(argv[1], "recovery") == 0)
        {
            syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                    LINUX_REBOOT_CMD_RESTART2, "recovery");
        }
        else if (strcmp(argv[1], "system") == 0)
        {
            syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                    LINUX_REBOOT_CMD_RESTART2, "system");
        }
        else if (strcmp(argv[1], "boot") == 0)
        {
            syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                    LINUX_REBOOT_CMD_RESTART2, "boot");
        }
        else
        {
            syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                    LINUX_REBOOT_CMD_RESTART2, "PMIC_cold_reboot");
        }
    }
    else
    {
        // Default: cold reboot
        syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2,
                "bootloader");
    }
    return 0;
}