#include <kernel.h>
#define BOOT_LOADER_SIG_OFFSET (0x1fe + 0x50200000)
#define BATCH_IO_BUFFER_LOC (BOOT_LOADER_SIG_OFFSET - 8)

int main(void) {
    int input_val = *(short *)(BATCH_IO_BUFFER_LOC);
    return input_val * input_val;
}
