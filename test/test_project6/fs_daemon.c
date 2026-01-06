#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

void sys_fs_sync(void);

int main(void)
{
    printf("FS Daemon Started. Sync freq: 30s.\n");
    while (1) {
        sys_sleep(30);
        sys_fs_sync();
    }
    return 0;
}
