#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define FILE_SIZE (512 * 1024) // 512KB
static char buff[4096];

void test_read_cache()
{
    printf("--- Read Cache Performance Test ---\n");
    int fd = sys_open("read_test.bin", O_RDWR);
    if (fd < 0) {
        printf("Creating test file...\n");
        fd = sys_open("read_test.bin", O_RDWR);
        for (int i = 0; i < FILE_SIZE / 4096; i++) {
            sys_f_write(fd, buff, 4096);
        }
    }
    sys_lseek(fd, 0, SEEK_SET);

    // First Read (Cold Cache)
    long start = sys_get_tick();
    for (int i = 0; i < FILE_SIZE / 4096; i++) {
        sys_read(fd, buff, 4096);
    }
    long end = sys_get_tick();
    printf("Cold Read Ticks: %ld\n", end - start);

    // Second Read (Warm Cache)
    sys_lseek(fd, 0, SEEK_SET);
    start = sys_get_tick();
    for (int i = 0; i < FILE_SIZE / 4096; i++) {
        sys_read(fd, buff, 4096);
    }
    end = sys_get_tick();
    printf("Warm Read Ticks: %ld\n", end - start);

    sys_close(fd);
}

void test_write_performance()
{
    printf("\n--- Write Performance Test ---\n");
    
    // Switch to Write-Through
    printf("Switching to Write-Through...\n");
    int cfd = sys_open("/proc/sys/vm", O_RDWR);
    sys_f_write(cfd, "page_cache_policy = 0\nwrite_back_freq = 30\n", 44);
    sys_close(cfd);
    sys_sleep(1); // Wait for sync if needed

    int fd = sys_open("write_test.bin", O_RDWR);
    long start = sys_get_tick();
    for (int i = 0; i < 128; i++) { // 512KB
        sys_f_write(fd, buff, 4096);
    }
    long end = sys_get_tick();
    printf("Write-Through Ticks: %ld\n", end - start);
    sys_close(fd);

    // Switch to Write-Back
    printf("Switching to Write-Back...\n");
    cfd = sys_open("/proc/sys/vm", O_RDWR);
    sys_f_write(cfd, "page_cache_policy = 1\nwrite_back_freq = 30\n", 44);
    sys_close(cfd);
    sys_sleep(1);

    fd = sys_open("write_test_wb.bin", O_RDWR);
    start = sys_get_tick();
    for (int i = 0; i < 128; i++) {
        sys_f_write(fd, buff, 4096);
    }
    end = sys_get_tick();
    printf("Write-Back Ticks: %ld\n", end - start);
    sys_close(fd);
}

void test_metadata()
{
    printf("\n--- Metadata Performance Test ---\n");
    printf("Creating 100 files (5000 might be too slow for current shell)...\n");
    int cfd = sys_open("/proc/sys/vm", O_RDWR);
    sys_f_write(cfd, "page_cache_policy = 0\nwrite_back_freq = 30\n", 44);
    char name[16];
    long start = sys_get_tick();
    for (int i = 0; i < 100; i++) {
        itoa(i, name, 16, 10);
        strcat(name, ".txt");
        int fd = sys_open(name, O_RDWR);
        sys_close(fd);
    }
    long end = sys_get_tick();
    printf("Time to create 100 files: %ld ticks\n", end - start);
}

int main(void)
{
    memset(buff, 'A', 4096);
    
    test_read_cache();
    test_write_performance();
    test_metadata();

    return 0;
}
