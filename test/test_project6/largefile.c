#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64];

int main(void)
{
    int fd = sys_open("large.bin", O_RDWR);
    if (fd < 0) {
        printf("Error: Cannot open large.bin\n");
        return -1;
    }

    char *msg1 = "Direct block data at 0";
    char *msg2 = "Single indirect data at 1MB";
    char *msg3 = "Double indirect data at 128MB";

    printf("Testing large file support...\n");

    // 1. Write to direct block (offset 0)
    printf("Writing to offset 0...\n");
    sys_f_write(fd, msg1, strlen(msg1));

    // 2. Write to single indirect (offset 1MB)
    // 1MB = 1024 * 1024 bytes. 
    printf("Writing to offset 1MB...\n");
    sys_lseek(fd, 1024 * 1024, SEEK_SET);
    sys_f_write(fd, msg2, strlen(msg2));

    // 3. Write to double indirect (offset 128MB)
    // 128MB = 128 * 1024 * 1024 bytes.
    printf("Writing to offset 128MB...\n");
    sys_lseek(fd, 128 * 1024 * 1024, SEEK_SET);
    sys_f_write(fd, msg3, strlen(msg3));

    // 4. Verify data
    printf("Verifying data...\n");

    sys_lseek(fd, 0, SEEK_SET);
    memset(buff, 0, 64);
    sys_read(fd, buff, strlen(msg1));
    printf("Read at 0: %s\n", buff);

    sys_lseek(fd, 1024 * 1024, SEEK_SET);
    memset(buff, 0, 64);
    sys_read(fd, buff, strlen(msg2));
    printf("Read at 1MB: %s\n", buff);

    sys_lseek(fd, 128 * 1024 * 1024, SEEK_SET);
    memset(buff, 0, 64);
    sys_read(fd, buff, strlen(msg3));
    printf("Read at 128MB: %s\n", buff);

    sys_close(fd);
    printf("Large file test completed!\n");

    return 0;
}
