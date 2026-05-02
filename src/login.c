#include <stdlib.h>
#include <unistd.h>
int main()
{
    system("/bin/busybox clear");
    execl("/bin/busybox", "busybox", "sh", "-l", (char*)NULL);
    return 0;
}