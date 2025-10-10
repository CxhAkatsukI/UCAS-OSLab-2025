### Preparations

Before we move on to our real tasks, let's do some preparation work first. We've already generated a compilation database `compile_commands.json`. We first add it to `.gitignore`.

Now, lets execute the following command to move `compile_command.json` out of Git's tracking, without removing the file:

```bash
git rm --cached compile_commands.json
```

Then, we can execute the following command to check whether `compile_commands.json` has been deleted from Git's index:

```bash
❯ git status
On branch Project1
Your branch is ahead of 'origin/Project1' by 1 commit.
  (use "git push" to publish your local commits)

Changes to be committed:
  (use "git restore --staged <file>..." to unstage)
        deleted:    compile_commands.json

Untracked files:
  (use "git add <file>..." to include in what will be committed)
        .asm-lsp.toml
        .cache/
        .gitignore
        prime_check
```

We shall add `.gitignore` by executing:

```bash
git add .gitignore
```

Now that everything is ready for commit:

```bash
❯ git status
On branch Project1
Your branch is ahead of 'origin/Project1' by 1 commit.
  (use "git push" to publish your local commits)

Changes to be committed:
  (use "git restore --staged <file>..." to unstage)
        new file:   .gitignore
        deleted:    compile_commands.json

Untracked files:
  (use "git add <file>..." to include in what will be committed)
        .asm-lsp.toml
        .cache/
        prime_check
```

We can execute `git commit` to commit our changes.

Let's push our changes by executing:

```bash
git push --set-upstream origin Project1
```

or, if we've already executed this command before, we can just simply use:

```bash
git push
```

> [!Note] About the `--set-upstream` flag
> The `--set-upstream` argument in `git push --set-upstream origin Project1` establishes a tracking relationship between our local `Project1` branch and the `Project1` branch on the `origin` remote.

### Making our first boot block

First, we can complete the required code in `arch/riscv/boot/bootblock.S`:

```nasm
  // TODO: [p1-task1] call BIOS to print string "It's bootblock!"
  la a0, boot_msg
  li a7, BIOS_PUTSTR
  call bios_func_entry
.data
	boot_msg: .sting "It's Chuxiao Han's bootloader!"
```

Then, we can do `make dirs` and `make elf` as required. Note that if we've executed `make` before, we can do `make clean` first. The commands and outputs are as follows:

```bash
❯ make clean
rm -rf ./build

❯ make elf
riscv64-unknown-linux-gnu-gcc -O2 -std=gnu11 -fno-builtin -nostdlib -nostdinc -Wall -mcmodel=medany -ggdb3 -I./arch/riscv/include -Wl,--defsym=TEXT_START=0x50200000 -T riscv.lds -o build/bootblock ./arch/riscv/boot/bootblock.S -e main
/opt/riscv64-linux/bin/../lib/gcc/riscv64-unknown-linux-gnu/15.1.0/../../../../riscv64-unknown-linux-gnu/bin/ld: cannot open output file build/bootblock: No such file or directory
collect2: error: ld returned 1 exit status
make: *** [Makefile:141: build/bootblock] Error 1

❯ make dirs

❯ make elf
riscv64-unknown-linux-gnu-gcc -O2 -std=gnu11 -fno-builtin -nostdlib -nostdinc -Wall -mcmodel=medany -ggdb3 -I./arch/riscv/include -Wl,--defsym=TEXT_START=0x50200000 -T riscv.lds -o build/bootblock ./arch/riscv/boot/bootblock.S -e main
/opt/riscv64-linux/bin/../lib/gcc/riscv64-unknown-linux-gnu/15.1.0/../../../../riscv64-unknown-linux-gnu/bin/ld: warning: build/bootblock has a LOAD segment with RWX permissions
riscv64-unknown-linux-gnu-gcc -O2 -std=gnu11 -fno-builtin -nostdlib -nostdinc -Wall -mcmodel=medany -ggdb3 -I./arch/riscv/include -Iinclude -Wl,--defsym=TEXT_START=0x50201000 -T riscv.lds -o build/main ./arch/riscv/kernel/head.S ./init/main.c ./arch/riscv/bios/common.c ./kernel/loader/loader.c ./libs/string.c
/opt/riscv64-linux/bin/../lib/gcc/riscv64-unknown-linux-gnu/15.1.0/../../../../riscv64-unknown-linux-gnu/bin/ld: warning: build/main has a LOAD segment with RWX permissions
riscv64-unknown-linux-gnu-gcc -O2 -std=gnu11 -fno-builtin -nostdlib -nostdinc -Wall -mcmodel=medany -ggdb3 -I./tiny_libc/include -I./arch/riscv/include -c arch/riscv/crt0/crt0.S -o build/crt0.o
riscv64-unknown-linux-gnu-gcc -O2 -std=gnu11 -fno-builtin -nostdlib -nostdinc -Wall -mcmodel=medany -ggdb3 -I./tiny_libc/include -o build/2048 ./build/crt0.o test/test_project1/2048.c -Wl,--defsym=TEXT_START=0x52000000 -T riscv.lds
riscv64-unknown-linux-gnu-gcc -O2 -std=gnu11 -fno-builtin -nostdlib -nostdinc -Wall -mcmodel=medany -ggdb3 -I./tiny_libc/include -o build/auipc ./build/crt0.o test/test_project1/auipc.c -Wl,--defsym=TEXT_START=0x52010000 -T riscv.lds
riscv64-unknown-linux-gnu-gcc -O2 -std=gnu11 -fno-builtin -nostdlib -nostdinc -Wall -mcmodel=medany -ggdb3 -I./tiny_libc/include -o build/bss ./build/crt0.o test/test_project1/bss.c -Wl,--defsym=TEXT_START=0x52020000 -T riscv.lds
/opt/riscv64-linux/bin/../lib/gcc/riscv64-unknown-linux-gnu/15.1.0/../../../../riscv64-unknown-linux-gnu/bin/ld: warning: build/bss has a LOAD segment with RWX permissions
riscv64-unknown-linux-gnu-gcc -O2 -std=gnu11 -fno-builtin -nostdlib -nostdinc -Wall -mcmodel=medany -ggdb3 -I./tiny_libc/include -o build/data ./build/crt0.o test/test_project1/data.c -Wl,--defsym=TEXT_START=0x52030000 -T riscv.lds
/opt/riscv64-linux/bin/../lib/gcc/riscv64-unknown-linux-gnu/15.1.0/../../../../riscv64-unknown-linux-gnu/bin/ld: warning: build/data has a LOAD segment with RWX permissions
```

Then, we execute:

```bash
cp createimage build/
```

Next, we'll create an image with the following command:

```bash
❯ chmod +x createimage && cd build && ../createimage --extended bootblock main && cd ..
0x50200000: bootblock
        segment 0
                offset 0x1053           vaddr 0x0000
                filesz 0x0072           memsz 0x0000
        segment 1
                offset 0x1000           vaddr 0x50200000
                filesz 0x0053           memsz 0x0053
                writing 0x0053 bytes
                padding up to 0x0200
0x50201000: main
        segment 0
                offset 0x156a           vaddr 0x0000
                filesz 0x0078           memsz 0x0000
        segment 1
                offset 0x1000           vaddr 0x50201000
                filesz 0x0558           memsz 0x0590
                writing 0x0590 bytes
                padding up to 0x0800
        segment 2
                offset 0x1334           vaddr 0x50201334
                filesz 0x0094           memsz 0x0094
                writing 0x0094 bytes
                padding up to 0x0a00
        segment 3
                offset 0x0000           vaddr 0x0000
                filesz 0x0000           memsz 0x0000
os_size: 4 sectors
```

This indicates a successful image creation.

After the image is created, we use `make run` to start `QEMU`. We shall type in `loadboot` command to see the desired result:

```
❯ make run
/home/stu/OSLab-RISC-V/qemu/riscv64-softmmu/qemu-system-riscv64 -nographic -machine virt -m 256M -kernel /home/stu/OSLab-RISC-V/u-boot/u-boot -bios none -drive if=none,format=raw,id=image,file=./build/image -device virtio-blk-device,drive=image -monitor telnet::45454,server,nowait -serial mon:stdio


U-Boot 2024.07UCAS_OS DASICS v3.1.0-00024-gf507a8a632-dirty (Sep 07 2025 - 00:28:01 +0000)

CPU:   rv64imafdch_zicbom_zicboz_zicsr_zifencei_zihintpause_zawrs_zfa_zca_zcd_zba_zbb_zbc_zbs_sstc_svadu
Model: riscv-virtio,qemu
DRAM:  256 MiB
In:    serial@10000000
Out:   serial@10000000
Err:   serial@10000000
Net:   No ethernet found.
Hit any key to stop autoboot:  0

Device 0: QEMU VirtIO Block Device
            Type: Hard Disk
            Capacity: 0.0 MB = 0.0 GB (5 x 512)
... is now current device
** Invalid partition 1 **
No ethernet found.
No ethernet found.

virtio read: device 0 block # 0, count 2 ... 2 blocks read: OK
=> loadboot
It's Chuxiao Han's bootloader...
QEMU: Terminated
```

### Loading and initializing Memory

The code and logic is as follows:

```nasm
  // TODO: [p1-task2] call BIOS to read kernel in SD card
  // a1: hold os_size
  // first load os_size in a1
  lh a1, os_size_loc
  // a0: param1, for mem_address
  // a1: param2, for num_of_block
  // a2: param3, for block_id
  // prepare to call `bios_sd_read`
  li a0, kernel
  li a2, 1
  li a7, BIOS_SDREAD
  jal bios_func_entry


  // TODO: [p1-task4] load task-related arguments and pass them to kernel

  ...

  // TODO: [p1-task2] jump to kernel to start UCAS-OS
  j kernel

```

### Clearing the `bss` Section

The `bss` relevant symbols are defined in `riscv.lds`. In this script, we can find the following symbols:

```
__bss_start
__BSS_END__
```

So, to clear the `bss` section, we shall utilize these two symbols. After cleaning that section, we shall set up the stack pointer and jump to kernel main function. The code is as follows:

```nasm
  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  // a0: start pointer and `CURRENT` pointer
  // a1: end pointer
  la a0, __bss_start
  la a1, __BSS_END__

  clear_bss_loop_start:

  sw zero, (a0)
  addi a0, a0, 4
  blt a0, a1, clear_bss_loop_start

  // set up the stack pointer
  li sp, KERNEL_STACK

  // jump to kernel `main` function
  jal main
```

### Reading From Console and Echoes Characters Back

We need to modify `init/main.c`, After `Hello OS!` and `buf` is printed out, we add the following code:

```C
    // Use BIOS API to read characters from console and echoes back ( •̀ ω •́ )✧)
    while (1) {
        char input = bios_getchar();
        bios_putchar(input);
    }
```

And the console will output the characters that we've just input.

## Task 3

### Implementing `write_img_info` function

For task3, we shall first implement the `write_img_info` function. This function writes metadata into the first sector (first 512 bytes) of our image file.

Before emtering the implementation of this function, we can define some macros for the address of these metadata:

```C
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define TASK_NUM_LOC (BOOT_LOADER_SIG_OFFSET - 4)
#define TASK_INFO_LOC (BOOT_LOADER_SIG_OFFSET - 6 - (TASK_MAXNUM * sizeof(task_info_t)))
```

What's more, we can design our `task_info_t` here:

```C
/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    uint16_t start_sector;
    uint16_t size;
} task_info_t;
```

In this function, we can use `fseek` to move pointer to the desired location of our image file pointer, and use `fwrite` to write information into those locations:

```C
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE * img)
{
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...

    // find position for TASK_INFO_LOC
    fseek(img, TASK_INFO_LOC, SEEK_SET);
    fwrite(taskinfo, sizeof(task_info_t), tasknum, img);
    // find position for TASK_NUM_LOC
    fseek(img, TASK_NUM_LOC, SEEK_SET);
    fwrite(&tasknum, sizeof(short), 1, img);
    // calc os_size, and find position for OS_SIZE_LOC
    short os_size = NBYTES2SEC(nbytes_kernel);
    fseek(img, OS_SIZE_LOC, SEEK_SET);
    fwrite(&os_size, sizeof(short), 1, img);
}
```

### Completing the `create_image()` Loop

In the main loop of this function, we shall write padding bytes for `bootblock`, `kernel` and `tasks`. 

Before implementing the function itself, we can first define some macros to specify the fixed numbers of sectors used by kernel and applications:

```C
#define FIXED_APP_SECTORS 15
#define FIXED_KERNEL_SECTORS 15
```

The implementation of padding bytes writing are as follows:

```C
        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */

        // write paddings for bootblock
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }

        // write paddings for kernel
        if (strcmp(*files, "main") == 0) {
            write_padding(img, &phyaddr, (1 + FIXED_KERNEL_SECTORS) * SECTOR_SIZE);
        }
 
        // write paddings for tasks
        if (strcmp(*files, "main") != 0 && strcmp(*files, "bootblock") != 0) {
            write_padding(img, &phyaddr, (1 + FIXED_KERNEL_SECTORS + (taskidx + 1) * FIXED_APP_SECTORS) * SECTOR_SIZE);
        }

```

In this function, we should also write information into the `taskinfo` array:

```C
        // write info into taskinfo[tasknum] struct
        if (taskidx >= 0) {
            taskinfo[taskidx] = (task_info_t)
                {
                    .size = FIXED_APP_SECTORS,
                    .start_sector = 1 + FIXED_KERNEL_SECTORS + taskidx * FIXED_APP_SECTORS
                };
        }
```

### Adding Support for `main` function

Note that `main.c` included `os/task.h`, so we shall re-define some relavant macros in this file (but with offset, because those content has been loaded into RAM):

```C
#define BOOT_LOADER_SIG_OFFSET (0x1fe + 0x50200000)
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define TASK_NUM_LOC (BOOT_LOADER_SIG_OFFSET - 4)
#define TASK_INFO_LOC (BOOT_LOADER_SIG_OFFSET - 6 - (TASK_MAXNUM * sizeof(task_info_t)))
```

Before actually loading the tasks, We can add a simple print sequence to see whether the data has been correctly loaded into the area we've specified:

```C
    int tasknum = *((short *)TASK_NUM_LOC);
    for (int i = 0; i < tasknum; i++) {
        bios_putchar('t');
    }
```

After executing `make all`, I executed `make run`. I got these outputs:

```
=> loadboot
It's Chuxiao Han's bootloader...
Hello OS!
bss check: t version: 2
tttt
```

Is it really 4? We shall refer to the image target in Makefile:

```Makefile
image: $(ELF_CREATEIMAGE) $(ELF_BOOT) $(ELF_MAIN) $(ELF_USER)
	cd $(DIR_BUILD) && ./$(<F) --extended $(filter-out $(<F), $(^F))
```

These are the arguments that have been passed to `createimage`:

1.  `$(ELF_CREATEIMAGE)`: This is `build/createimage`.
2.  `$(ELF_BOOT)`: This is `build/bootblock`.
3.  `$(ELF_MAIN)`: This is `build/main`.
4.  `$(ELF_USER)`: This is derived from `$(wildcard $(DIR_TEST_PROJ)/*.c)`.
    *   `DIR_TEST_PROJ` is `test/test_project1`.
    *   Looking at the folder structure, `test/test_project1` contains: `2048.c`, `auipc.c`, `bss.c`, `data.c`.
    *   So, `$(ELF_USER)` will expand to `build/2048 build/auipc build/bss build/data`.

The command executed for the `image` target effectively becomes:
`cd build && ./createimage --extended bootblock main 2048 auipc bss data`

Counting the files passed to `createimage` (excluding the `--extended` option):
*   `bootblock` (1)
*   `main` (1)
*   `2048` (1)
*   `auipc` (1)
*   `bss` (1)
*   `data` (1)

Total number of files (`nfiles` in `create_image`) is 6.

In `createimage.c`, `tasknum` is calculated as `nfiles - 2`.
So, `tasknum = 6 - 2 = 4`.

This is what we're expecting, meaning that we've implemented `createimage.c` successfully. 

Next, we're going to implement a more beautiful print message. First, we define `itoa` in `string.c` and declare the function in `string.h`:

```C
// implement a simple itoa function to print loaded task num
char *itoa(int value, char *buffer, int base) {
    if (base != 10 || value > INT32_MAX)
        return NULL;
    if (value == 0) {
        *(buffer) = '0';
        *(buffer + 1) = '\0';
        return buffer;
    }
    char temp_buf[32];
    int temp_ptr = 31;
    while (value > 0) {
        temp_buf[temp_ptr] = value % base + '0';
        value = value / base;
        temp_ptr--;
    }
    temp_ptr++;
    char *copy_ptr = buffer;
    while (temp_ptr < 32) {
        *copy_ptr = temp_buf[temp_ptr];
        copy_ptr++;
        temp_ptr++;
    }
    *(copy_ptr) = '\0';
    return buffer;
}
```

We can also define some useful coloring macros in `string.h`:

```C
// formatting
#define ANSI_FG_BLACK "\33[1;30m"
#define ANSI_FG_RED "\33[1;31m"
#define ANSI_FG_GREEN "\33[1;32m"
#define ANSI_FG_YELLOW "\33[1;33m"
#define ANSI_FG_BLUE "\33[1;34m"
#define ANSI_FG_MAGENTA "\33[1;35m"
#define ANSI_FG_CYAN "\33[1;36m"
#define ANSI_FG_WHITE "\33[1;37m"
#define ANSI_BG_BLACK "\33[1;40m"
#define ANSI_BG_RED "\33[1;41m"
#define ANSI_BG_GREEN "\33[1;42m"
#define ANSI_BG_YELLOW "\33[1;43m"
#define ANSI_BG_BLUE "\33[1;44m"
#define ANSI_BG_MAGENTA "\33[1;45m"
#define ANSI_BG_CYAN "\33[1;46m"
#define ANSI_BG_WHITE "\33[1;47m"
#define ANSI_NONE "\33[0m"

// macro stringizing
#define str_temp(x) #x
#define str(x) str_temp(x)
#define ANSI_FMT(str, fmt) fmt str ANSI_NONE
```

Then, we can produce beautiful coloring messages in `main.c`, like task number printing:

```C
    int tasknum = *((short *)TASK_NUM_LOC);
    char temp_buf[] = "_____";
    char *tasknum_buf = itoa(tasknum, temp_buf, 10);

    // Construct the message with color
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Loaded ", ANSI_FG_GREEN));
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr(tasknum_buf);
    bios_putstr(ANSI_NONE);
    bios_putstr(ANSI_FMT(" tasks.", ANSI_FG_GREEN));
```

Next, we'll implement an interactive user input interface, to allow users input the task that they want to execute:

```C
    // Prompt the user to input the task that he want to execute
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
    bios_putstr(ANSI_FMT("Please enter the task to execute: \n", ANSI_FG_YELLOW));
    bios_putstr(ANSI_FMT("~> ", ANSI_FG_CYAN));


    // Use BIOS API to read characters from console and echoes back ( •̀ ω •́ )✧)
    int task_idx = 0;
    char temp_task_idx_buf[] = "_____";
    char *exec_task_idx_buf;
    while (1) {
        char input = bios_getchar();
        if (input == '\n' || input == '\r') {
            bios_putchar('\n');
            bios_putchar('\r');
            if (task_idx >= tasknum || task_idx < 0) {
                // Prompt the user to input the task that he want to execute
                bios_putstr(ANSI_FMT("ERROR: Invalid task index", ANSI_BG_RED));
                bios_putstr(ANSI_FMT("\n\rInfo: ", ANSI_FG_BLUE));
                bios_putstr(ANSI_FMT("Please enter the task to execute: \n", ANSI_FG_YELLOW));
                bios_putstr(ANSI_FMT("~> ", ANSI_FG_CYAN));
                task_idx = 0;
                continue;
            }
            exec_task_idx_buf = itoa(task_idx, temp_task_idx_buf, 10);
            break;
        }
        if (input != 0xFF) {
            task_idx *= 10;
            task_idx += input - '0';
            bios_putchar(input);
        }
    }

    // Print success message
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Now executing task ", ANSI_FG_GREEN));
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr(exec_task_idx_buf);
    bios_putstr(ANSI_NONE);
    bios_putstr(ANSI_FMT("\n", ANSI_FG_GREEN));
```

### Revising `task_info_t` and Enabling Detecting Tasks by Name

We shall include the `name` property in `task_info_t`, as follows:

```C
/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    char name[MAX_NAME_LEN];
    uint16_t start_sector;
    uint16_t size;
} task_info_t;
```

When initializing task info in the main function, we should copy the `task_info` array that has been written to the first 512 Bytes of RAM to the array that we define in `main.c`:

```C
static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    int tasknum = *(uint16_t *)TASK_NUM_LOC;

    for (int i = 0; i < tasknum; i++) {
        // Read the task information from the specified memory location
        tasks[i] = *(task_info_t *)(TASK_INFO_LOC + i * sizeof(task_info_t));

        // Conditional debug output block
        if (DEBUG == 1) {
            // Set the text color to green
            bios_putstr(ANSI_FG_GREEN);

            // Print the debug message in parts
            bios_putstr("DEBUG: task detected, '");
            bios_putstr(tasks[i].name);
            bios_putstr("'\n\r");

            // Reset the color back to default
            bios_putstr(ANSI_NONE);
        }

    }
}
```

A helper function is defined to match task name:

```C
static int search_task_name(int tasknum, char name[]) {
    for (int i = 0; i < tasknum; i++) {
        if (strcmp(name, tasks[i].name) == 0)
            return i;
    }
    return -1;
}
```

The following function could recognize task on both index input or name input. When you input `1` or `auipc`, this function will always know the application that you want to execute:

```C
    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    int tasknum = *((short *)TASK_NUM_LOC);
    char temp_buf[] = "_____";
    char *tasknum_buf = itoa(tasknum, temp_buf, 10);

    // Construct the message with color
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Loaded ", ANSI_FG_GREEN));
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr(tasknum_buf);
    bios_putstr(ANSI_NONE);
    bios_putstr(ANSI_FMT(" tasks.\n", ANSI_FG_GREEN));

    // Prompt the user to input the task that he want to execute
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
    bios_putstr(ANSI_FMT("Please enter the task to execute: \n", ANSI_FG_YELLOW));
    bios_putstr(ANSI_FMT("~> ", ANSI_FG_CYAN));


    // Use BIOS API to read characters from console and echoes back ( •̀ ω •́ )✧)
    int task_idx = 0;
    char temp_task_idx_buf[] = "_____";
    char temp_task_name_buf[32];
    int task_name_buf_ptr = 0;
    char *exec_task_idx_buf;
    while (1) {
        char input = bios_getchar();
        if (input == '\n' || input == '\r') {
            bios_putchar('\n');
            bios_putchar('\r');
            temp_task_name_buf[task_name_buf_ptr] = '\0';
            int task_idx_by_name = search_task_name(tasknum, temp_task_name_buf);
            if (task_idx_by_name != -1) {
                task_idx = task_idx_by_name;
                exec_task_idx_buf = itoa(task_idx, temp_task_idx_buf, 10);
                break;
            } else if (task_idx >= tasknum || task_idx < 0) {
                // Prompt the user to input the task that he want to execute
                bios_putstr(ANSI_FMT("ERROR: Invalid task index or name", ANSI_BG_RED));
                bios_putstr(ANSI_FMT("\n\rInfo: ", ANSI_FG_BLUE));
                bios_putstr(ANSI_FMT("Please enter the task to execute: \n", ANSI_FG_YELLOW));
                bios_putstr(ANSI_FMT("~> ", ANSI_FG_CYAN));

                // reset index and name buf pointer
                task_idx = 0;
                task_name_buf_ptr = 0;
                continue;
            } else {
                exec_task_idx_buf = itoa(task_idx, temp_task_idx_buf, 10);
                break;
            }
        }
        if (input != 0xFF) {
            task_idx *= 10;
            task_idx += input - '0';
            temp_task_name_buf[task_name_buf_ptr++] = input;
            bios_putchar(input);
        }
    }

    // Print success message
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Now executing task ", ANSI_FG_GREEN));
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr(exec_task_idx_buf);
    bios_putstr(", ");
    bios_putstr(tasks[task_idx].name);
    bios_putstr(ANSI_NONE);
    bios_putstr(ANSI_FMT("\n", ANSI_FG_GREEN));
```

The next task is to enable compact arrangement of the image file, and loading the user specified application correctly. 

For this task, we should first add some fields to `task_info_t` to provide necessary info for the loader:

```C
typedef struct {
    char name[MAX_NAME_LEN];
    uint16_t start_sector;
    uint16_t size;
    uint32_t byte_offset;
    uint32_t byte_size;
} task_info_t;
```

We shall modify the `create_image` loop to fill in these information:

```C
/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 2;
    int nbytes_kernel = 0;
    int nbytes_application = 0;
    int phyaddr = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /* for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 2;
        int phyaddr_start_of_file = phyaddr;
        nbytes_application = 0;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            if (phdr.p_type != PT_LOAD) continue;

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);

            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0) {
                nbytes_kernel += get_filesz(phdr);
            }

            /* update nbytes_application */
            if (strcmp(*files, "main") != 0 && strcmp(*files, "bootblock") != 0) {
                nbytes_application += get_filesz(phdr);
            }
        }

        // write info into taskinfo[tasknum] struct
        if (taskidx >= 0) {
            strncpy(taskinfo[taskidx].name, *files, MAX_NAME_LEN);
            taskinfo[taskidx].name[MAX_NAME_LEN - 1] = '\0';
            taskinfo[taskidx].size = NBYTES2SEC(nbytes_application),
            taskinfo[taskidx].start_sector = phyaddr_start_of_file / SECTOR_SIZE;
            taskinfo[taskidx].byte_offset = phyaddr_start_of_file;
            taskinfo[taskidx].byte_size = nbytes_application;
        }

        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */

        // write paddings for bootblock
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }

        // write paddings for kernel
        // if (strcmp(*files, "main") == 0) {
        //     write_padding(img, &phyaddr, (1 + FIXED_KERNEL_SECTORS) * SECTOR_SIZE);
        // }
 
        // write paddings for tasks
        // if (strcmp(*files, "main") != 0 && strcmp(*files, "bootblock") != 0) {
        //    write_padding(img, &phyaddr, (1 + FIXED_KERNEL_SECTORS + (taskidx + 1) * FIXED_APP_SECTORS) * SECTOR_SIZE);
        // }

        fclose(fp);
        files++;
    }
    write_img_info(nbytes_kernel, taskinfo, tasknum, img);

    fclose(img);
}
```

Then, we should implement `loader.c`, allowing the main function to load the specified application into the RAM area begins with `TASK_MEM_BASE`:

```C
uint64_t load_task_img(char *name, int tasknum)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    task_info_t task = tasks[search_task_name(tasknum, name)];
    sd_read((uintptr_t)temp_load_buffer, task.size, task.start_sector);
    uint32_t offset_in_buffer = task.byte_offset % SECTOR_SIZE;
    memcpy((void *)TASK_MEM_BASE, (void *)temp_load_buffer + offset_in_buffer, task.byte_size);

    // Conditional debug output block
    if (DEBUG == 1) {
        // Set the text color to green
        bios_putstr(ANSI_FG_GREEN);

        bios_putstr("DEBUG: Loaded '");
        bios_putstr(name);
        bios_putstr("'. First bytes in memory:\n\r  "); // Indent the hex output

        // Determine how many bytes to print (up to a max of 16 for a brief summary)
        int bytes_to_print = (task.byte_size > 16) ? 16 : task.byte_size;
        uint8_t *mem_ptr = (uint8_t *)TASK_MEM_BASE;

        // Loop through the bytes and print each one in hex
        for (int i = 0; i < bytes_to_print; i++) {
            bios_puthex_byte(mem_ptr[i]);
            bios_putchar(' ');
        }

        // as it would be redundant for smaller files.
        if (task.byte_size > 16) {
            bios_putstr("\n\r  Last bytes in memory:\n\r  ");

            // Point to the start of the last 16 bytes
            uint8_t *last_mem_ptr = (uint8_t *)TASK_MEM_BASE + task.byte_size - 16;

            // Loop through the last 16 bytes and print each one in hex
            for (int i = 0; i < 16; i++) {
                bios_puthex_byte(last_mem_ptr[i]);
                bios_putchar(' ');
            }
        }

        bios_putstr("\n\r");

        // IMPORTANT: Reset the color back to default
        bios_putstr(ANSI_NONE);
    }

    return TASK_MEM_BASE;
}
```

Note, here a helper function `bios_puthex_byte` is defined to print bytes as debug info:

```C
/**
 * @brief Prints a single byte as a two-digit hexadecimal value.
 *
 * @param byte The byte to print.
 */
static inline void bios_puthex_byte(uint8_t byte)
{
    // A lookup table for hexadecimal characters
    const char hex_chars[] = "0123456789abcdef";
    // Print the high nibble (first 4 bits)
    bios_putchar(hex_chars[(byte >> 4) & 0x0F]);
    // Print the low nibble (last 4 bits)
    bios_putchar(hex_chars[byte & 0x0F]);
}
```

### Implementing `crt0.S`

`crt0.S` is used for setting up C runtime environment for user programs, including clearing `bss` section, entering user `main` function, and returning to the kernel after the task has finished. The implementation of this file is as follows:

```C
#include <asm.h>
#define USER_STACKPTR     0x52010000
#define TASK_MEM_BASE     0x52000000
#define KERNEL_ENTRYPOINT 0x50201000

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */

    la a0, __bss_start
    la a1, __BSS_END__

    clear_bss_loop_start:

    sw zero, (a0)
    addi a0, a0, 4
    blt a0, a1, clear_bss_loop_start

    li sp, USER_STACKPTR

    /* TODO: [p1-task3] enter main function */

    la a0, main
    jalr a0


    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    li t0, KERNEL_ENTRYPOINT
    jr t0

    /************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
```

> [!Note] The Use of `jr` and `jalr`
> We use these two command here because the jump distance is quite far. If we simply use `j` and `jal`, we'll receive the following error:

```C
./build/crt0.o: in function `clear_bss_loop_start':
/home/stu/hanchuxiao23/arch/riscv/crt0/crt0.S:25:(.entry_function+0x28): relocation truncated to fit: R_RISCV_JAL against `*UND*'
collect2: error: ld returned 1 exit status
make: *** [Makefile:150: build/2048] Error 1
```

### Modifying `sd_read` logic

After implementing all the relevant functions, when I executed task 2, `bss`, I encountered the following problem: 

```
[U-BOOT] ERROR: truly_illegal_insn
exception code: 2 , Illegal instruction , epc 52000054 , ra 52000028
### ERROR ### Please RESET the board ###
```

And GDB shows the following information:

```
(gdb) x/20i 0x5200002e
   0x5200002e:  wfi
   0x52000032:  j       0x5200002e
   0x52000034:  nop
   0x52000036:  addi    sp,sp,-16
   0x52000038:  sd      ra,8(sp)
   0x5200003a:  auipc   a5,0x0
   0x5200003e:  addi    a5,a5,254
   0x52000042:  auipc   a3,0x0
   0x52000046:  addi    a3,a3,296
   0x5200004a:  j       0x52000050
   0x5200004c:  beq     a5,a3,0x52000066
   0x52000050:  lbu     a4,0(a5)
=> 0x52000054:  unimp
   0x52000056:  unimp
   0x52000058:  unimp
   0x5200005a:  unimp
   0x5200005c:  unimp
   0x5200005e:  unimp
   0x52000060:  unimp
   0x52000062:  unimp
```

The content above the problematic part is the same as the disassembly file, however, the RAM area below 0x52000054 shows `unimp`. Then, I suspect that the user application hasn't been completely copied. And I find the following issue:

I used this in my original code:

```C
    sd_read((uintptr_t)temp_load_buffer, task.size, task.start_sector);
```

However, this approach cannot face the following situation:

If a program is less than 1 sector in total, but half of it is on sector 17, another half is on sector 18 (like `bss`), as shown in the following situation:

```
+----------+-----+-----+----------+----------------+----------------+
|          |     |     |          |                |                |
|       17 |  PROGRAM  | 18       |       19       |       20       |      
|          |     |     |          |                |                |
+----------+-----+-----+----------+----------------+----------------+
```

Then, the code above will only copy the part from the 17th sector. That's something we don't want, so we modify it into the following code:

```C
    sd_read((uintptr_t)temp_load_buffer, task.size + 1, task.start_sector);
```

## Task 5

### Wrap up existing functions

Before we implement task name printing, we're going to wrap up existing `user_input_and_launch_task` function into a handler: 

```C
uint64_t user_input_and_launch_task_handler(int tasknum) {
    // Prompt the user to input the task that he want to execute
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
    bios_putstr(ANSI_FMT("Please enter the task to execute: \n", ANSI_FG_YELLOW));
    bios_putstr(ANSI_FMT("~> ", ANSI_FG_CYAN));


    // Use BIOS API to read characters from console and echoes back ( •̀ ω •́ )✧)
    int task_idx = 0;
    char temp_task_idx_buf[] = "_____";
    char temp_task_name_buf[32];
    int task_name_buf_ptr = 0;
    char *exec_task_idx_buf;
    while (1) {
        char input = bios_getchar();
        if (input == '\n' || input == '\r') {
            bios_putchar('\n');
            bios_putchar('\r');
            temp_task_name_buf[task_name_buf_ptr] = '\0';
            int task_idx_by_name = search_task_name(tasknum, temp_task_name_buf);
            if (task_idx_by_name != -1) {
                task_idx = task_idx_by_name;
                exec_task_idx_buf = itoa(task_idx, temp_task_idx_buf, 10);
                break;
            } else if (task_idx >= tasknum || task_idx < 0) {
                // Prompt the user to input the task that he want to execute
                bios_putstr(ANSI_FMT("ERROR: Invalid task index or name", ANSI_BG_RED));
                bios_putstr(ANSI_FMT("\n\rInfo: ", ANSI_FG_BLUE));
                bios_putstr(ANSI_FMT("Please enter the task to execute: \n", ANSI_FG_YELLOW));
                bios_putstr(ANSI_FMT("~> ", ANSI_FG_CYAN));

                // reset index and name buf pointer
                task_idx = 0;
                task_name_buf_ptr = 0;
                continue;
            } else {
                exec_task_idx_buf = itoa(task_idx, temp_task_idx_buf, 10);
                break;
            }
        }
        if (input != 0xFF) {
            task_idx *= 10;
            task_idx += input - '0';
            temp_task_name_buf[task_name_buf_ptr++] = input;
            bios_putchar(input);
        }
    }

    // Print success message
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Now executing task ", ANSI_FG_GREEN));
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr(exec_task_idx_buf);
    bios_putstr(", ");
    bios_putstr(tasks[task_idx].name);
    bios_putstr(ANSI_NONE);
    bios_putstr(ANSI_FMT("\n", ANSI_FG_GREEN));

    // Print success message
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Windows is loading files...\n\r", ANSI_FG_GREEN));
    uint64_t entry_point = load_task_img(tasks[task_idx].name, tasknum);

    // enter the entry point
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Starting task...\n\r", ANSI_FG_GREEN));
    ((void (*)(void))entry_point)();

    return 0;
}
```

However, this simple modification introduced a bug:

```
Info: Windows is loading files...
    blocks read error!
    DEBUG: Loaded 'data'. First bytes in memory:
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    Last bytes in memory:
    00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    Info: Starting task...
    [U-BOOT] ERROR: truly_illegal_insn
    exception code: 2 , Illegal instruction , epc 52000000 , ra 
    50201570
    ### ERROR ### Please RESET the board ###
```

The `sd_read` doesn't work! However, for task `0`, `1` and `2`, it works pretty fine. And when I printed `task.size` and `task.start_sector` out, the error will disappear.

> [!Warning]
> This error is solved by removing the `+1` fix that we've just done!! I think this part will introduce some bugs in the future, but I currently don't have an idea about it.

### Implementing Command Line Parsing Scheme

To implement task listing and batch processing, we shall have a command line parsing scheme to deal with command line inputs efficiently, and should also have a better extensibility. The following is my implementation:

Firstly, i defined a `cmd_table` to hold all valid commands and their handler:

```C
typedef struct {
    char *name;
    char *description;
    int (*handler)(char *);
} command_t;

// Command table for all the available commands
command_t cmd_table[] = {
    {"help", "Display information about all supported commands", cmd_help},
    {"ls", "List all loaded applications", cmd_ls},
    {"exec", "Execute a task by name or ID", cmd_exec},
    {"write_batch", "Write a batch processing sequence to image", cmd_write_batch},
    {"exec_batch", "Execute the stored batch processing sequence", cmd_exec_batch}
};
```

We define some helper functions:

```C
/**
 * @brief Search the command table based on given command name.
 */
int search_command_table(char buf[]) {
    for (int i = 0; i < NR_CMD; i++) {
        if (strcmp(buf, cmd_table[i].name) == 0)
            return i;
    }
    return -1;
}

/**
 * Reads a line of input from the console.
 *
 * @param buffer   The character array to store the input.
 * @param max_len  The maximum number of characters to read (size of the buffer).
 * @return         0 on success.
 */
static int read_line(char *buffer, int max_len) {
    int ptr = 0;

    while (1) {
        // Read a single character from the BIOS/console
        char input_char = bios_getchar();

        // Check for Enter key (newline or carriage return)
        if (input_char == '\n' || input_char == '\r') {
            bios_putchar('\n');
            bios_putchar('\r');
            buffer[ptr] = '\0'; // Null-terminate the string
            return 0;           // Success
        }

        // 0xFF typically means no character was available, so we ignore it
        if (input_char != 0xFF) {
            // Handle backspace
            if (input_char == '\b' || input_char == 127) {
                if (ptr > 0) {
                    ptr--;
                    // Erase the character from the screen
                    bios_putchar('\b');
                    bios_putchar(' ');
                    bios_putchar('\b');
                }
            }
            // Handle regular characters, ensuring no buffer overflow
            else if (ptr < max_len - 1) {
                buffer[ptr++] = input_char;
                bios_putchar(input_char); // Echo the character back to the user
            }
        }
    }
}

/**
 * @brief Helper function to tokenize a string into an array of strings.
 *
 * This function parses an input string `input_str` based on space and tab delimiters.
 * Each found token is copied into the `tokens` array.
 *
 * @param input_str The string to tokenize.
 * @param tokens A 2D char array to store the resulting tokens.
 * @param max_tokens The maximum number of tokens to extract.
 * @return The number of tokens found, or -1 on error (e.g., a token is too long).
 */
static int tokenize_string(char *input_str, char tokens[][MAX_NAME_LEN], int max_tokens) {
    int token_count = 0;
    char *current_char = input_str;

    // Handle null or empty input string gracefully
    if (input_str == NULL || *input_str == '\0') {
        return 0; // No tokens to parse
    }

    // --- Main tokenization loop ---
    // Continue as long as we haven't reached the end of the string or the token limit
    while (*current_char != '\0' && token_count < max_tokens) {
        // 1. Skip any leading whitespace (spaces or tabs)
        while (*current_char == ' ' || *current_char == '\t') {
            current_char++;
        }

        // If we've reached the end of the string after skipping whitespace, exit
        if (*current_char == '\0') {
            break;
        }

        // 2. Identify the start and length of the next token
        char *token_start = current_char;
        int token_len = 0;
        // A token is a sequence of non-whitespace, non-null characters
        while (*current_char != '\0' && *current_char != ' ' && *current_char != '\t') {
            current_char++;
            token_len++;
        }

        // 3. Copy the token into the output array
        // Check for buffer overflow before copying
        if (token_len >= MAX_NAME_LEN) {
            bios_putstr(ANSI_FMT("ERROR: Task name too long for batch.", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
            return -1; // Indicate error
        }

        strncpy(tokens[token_count], token_start, token_len);
        tokens[token_count][token_len] = '\0'; // Manually null-terminate the copied string
        token_count++;
    }
    return token_count;
}

/**
 * @brief Parses a buffer containing newline-separated task names into an array.
 *
 * This function iterates through a character buffer, treating each line
 * (separated by '\n' or '\r') as a single task name.
 *
 * @param buffer The input character buffer read from the batch file.
 * @param tasks_array The 2D array to store the parsed task names.
 * @param max_tasks The maximum number of tasks to parse.
 * @return The number of tasks successfully parsed, or -1 on error.
 */
int parse_batch_file(char *buffer, char tasks_array[][MAX_NAME_LEN], int max_tasks) {
    int task_count = 0;
    char *current_char = buffer;

    // Handle null or empty buffer
    if (buffer == NULL || *buffer == '\0') {
        return 0;
    }

    // Loop until the end of the buffer or the task limit is reached
    while (*current_char != '\0' && task_count < max_tasks) {
        // 1. Skip any leading newlines or carriage returns to find the start of a line
        while (*current_char == '\n' || *current_char == '\r') {
            current_char++;
        }

        // If we reached the end of the buffer after skipping newlines, stop
        if (*current_char == '\0') {
            break;
        }

        // 2. Identify the start and length of the task name on the current line
        char *name_start = current_char;
        int name_len = 0;
        while (*current_char != '\0' && *current_char != '\n' && *current_char != '\r') {
            current_char++;
            name_len++;
        }

        // 3. Copy the task name into the output array
        // Check for buffer overflow before copying
        if (name_len >= MAX_NAME_LEN) {
            bios_putstr(ANSI_FMT("ERROR: Task name in batch file too long.", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
            return -1; // Indicate error
        }

        strncpy(tasks_array[task_count], name_start, name_len);
        tasks_array[task_count][name_len] = '\0'; // Manually null-terminate the string
        task_count++;
    }
    return task_count;
}

```

The main loop that is used to parse commands are as follows:

```C
/**
 * @brief Runs the main interactive command shell loop.
 */
void run_command_loop() {
    while (1) {
        // 1. Print prompt and read user input
        bios_putstr(ANSI_FMT("(cmd) ", ANSI_FG_CYAN));
        char temp_cmd_buf[32] = {0};
        read_line(temp_cmd_buf, MAX_INPUT_LEN);

        // 2. Parse the input buffer into a command and an argument string.
        char * args = NULL;
        int i = 0;
        while (temp_cmd_buf[i] != '\0') {
            if (temp_cmd_buf[i] == ' ') {
                temp_cmd_buf[i] = '\0';
                args = &temp_cmd_buf[i + 1];

                // This handles inputs like "exec   2048" correctly.
                while (*args == ' ') {
                    args++;
                }

                if (*args == '\0') {
                    args = NULL;
                }

                break;
            }
            i++;
        }

        // 3. Find and execute the corresponding command handler.
        char *command = temp_cmd_buf;
        int cmd_idx = search_command_table(command);
        if (cmd_idx == -1) {
            // Prompt the user to input the task that he want to execute
            bios_putstr(ANSI_FMT("ERROR: Invalid command, try `help`...", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
        } else {
            cmd_table[cmd_idx].handler(args);
        }
    }
}
```

Then, we will implement each handler:

> [!Note] The Implementation of Batch Processing
> The implmentation of `write_batch` and `exec_batch` is as follows. Since once a task is finished, the kernel will restart, so I choose to write `IN_BATCH_MODE`, `BATCH_TASK_INDEX` (current task), `BATCH_TOTAL_TASKS` in fixed memory location. Therefore, the kernel will see it even after it restarts.

```C
/**
 * Command handler for 'ls'.
 * List all tasks.
 */
int cmd_ls(char *args) {
    // The 'args' parameter is ignored for this command.
    bios_putstr("Info: Listing tasks:\n\r");

    for (int i = 0; i < tasknum; ++i) {
        char index_str[5]; // Buffer to hold the string version of the index
        bios_putstr("  [");
        bios_putstr(itoa(i, index_str, 10));
        bios_putstr("] ");
        bios_putstr(tasks[i].name);
        bios_putstr("\n\r");
    }
    return 0; // Indicate success
}

/**
 * Command handler for 'exec'.
 * Executes a task by its numerical ID or by its name.
 */
int cmd_exec(char *args) {
    if (args == NULL || *args == '\0') {
        bios_putstr(ANSI_FMT("ERROR: Usage: exec <task_name_or_id>", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
        return 0;
    }

    int selected_task_idx = -1;

    // First, try to parse the argument as a number (task ID).
    int numeric_val = 0;
    bool is_numeric = true;
    char *p = args;
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            is_numeric = false;
            break;
        }
        numeric_val = numeric_val * 10 + (*p - '0');
        p++;
    }

    // If it was a valid number, check if it's a valid task index.
    if (is_numeric) {
        if (numeric_val >= 0 && numeric_val < tasknum) {
            selected_task_idx = numeric_val;
        }
    }

    // If it wasn't a valid number or a valid index, try to find it by name.
    if (selected_task_idx == -1) {
        selected_task_idx = search_task_name(tasknum, args);
    }

    // If a task was found (either by ID or name), execute it.
    if (selected_task_idx != -1) {
        char index_str[5];

        bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
        bios_putstr(ANSI_FMT("Now executing task ", ANSI_FG_GREEN));

        bios_putstr(ANSI_FG_CYAN);
        bios_putstr(itoa(selected_task_idx, index_str, 10));
        bios_putstr(ANSI_NONE);
        bios_putstr(ANSI_FMT(", ", ANSI_FG_GREEN));
        bios_putstr(ANSI_FG_CYAN);
        bios_putstr(tasks[selected_task_idx].name);
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.

        bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
        bios_putstr(ANSI_FMT("Windows is loading files...\n\r", ANSI_FG_GREEN));

        uint64_t entry_point = load_task_img(tasks[selected_task_idx].name, tasknum);

        bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
        bios_putstr(ANSI_FMT("Starting task...\n\r", ANSI_FG_GREEN));

        ((void (*)(void))entry_point)();

        bios_putstr(ANSI_FMT("\nInfo: Task finished. Returning to shell.\n\r", ANSI_FG_GREEN));

    } else {
        bios_putstr(ANSI_FMT("ERROR: Invalid task index or name: ", ANSI_BG_RED));
        bios_putstr(args); // Show the invalid input
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
    }

    return 0; // Indicate success
}

/**
 * Command handler for 'help'.
 * Displays a list of supported commands and their descriptions.
 */
int cmd_help(char *args) {
    bios_putstr("Info: Supported commands:\n\r");
    for (int i = 0; i < NR_CMD; ++i) {
        bios_putstr("  ");
        bios_putstr(cmd_table[i].name);
        bios_putstr(": ");
        bios_putstr(cmd_table[i].description);
        bios_putstr("\n\r");
    }
    return 0; // Indicate success
}

/**
 * @brief Command handler to write a sequence of task names to a batch file on an SD card.
 *
 * Parses task names from the `args` string, validates each one, and writes them
 * line-by-line into a buffer which is then written to the SD card.
 *
 * @param args A space-separated string of task names.
 * @return Always returns 0.
 */
int cmd_write_batch(char *args) {
    // Check for empty arguments
    if (args == NULL || *args == '\0') {
        bios_putstr(ANSI_FMT("ERROR: Usage: write_batch <task_name1> <task_name2> ...", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
        return 0;
    }

    // --- Tokenize the input arguments into individual task names ---
    char parsed_names[MAX_BATCH_TASKS][MAX_NAME_LEN];
    int num_parsed_tasks = tokenize_string(args, parsed_names, MAX_BATCH_TASKS);

    // Handle errors from the tokenizer
    if (num_parsed_tasks == -1) { // An error occurred, message already printed
        return 0;
    }
    if (num_parsed_tasks == 0) {
        bios_putstr(ANSI_FMT("ERROR: No tasks provided for batch.", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
        return 0;
    }

    // --- Validate tasks and format them into the batch buffer ---
    int buffer_ptr = 0;
    memset(batch_sequence_buffer, 0, sizeof(batch_sequence_buffer)); // Clear buffer before use

    for (int i = 0; i < num_parsed_tasks; ++i) {
        // Check if the task name is valid by searching a global list
        if (search_task_name(tasknum, parsed_names[i]) == -1) {
            bios_putstr(ANSI_FMT("ERROR: Invalid task name in batch: ", ANSI_BG_RED));
            bios_putstr(parsed_names[i]);
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
            return 0; // Abort the entire operation if any task is invalid
        }

        // Append the valid task name and a newline to the buffer
        int name_len = strlen(parsed_names[i]);
        // Ensure there is enough space in the buffer for the name and newline
        if (buffer_ptr + name_len + 1 >= BATCH_FILE_SIZE_SECTORS * SECTOR_SIZE) {
            bios_putstr(ANSI_FMT("ERROR: Batch file buffer full.", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
            return 0;
        }
 
        memcpy((uint8_t *)batch_sequence_buffer + buffer_ptr, (uint8_t *)parsed_names[i], name_len);
        buffer_ptr += name_len;
        batch_sequence_buffer[buffer_ptr++] = '\n'; // Use newline as a separator
    }
    batch_sequence_buffer[buffer_ptr] = '\0'; // Null-terminate the entire buffer content

    // --- Write the prepared buffer to the SD card ---
    int write_ret = bios_sd_write((uintptr_t)batch_sequence_buffer, BATCH_FILE_SIZE_SECTORS, g_batch_file_start_sector);

    if (write_ret == 0) { // Assuming 0 indicates success
        bios_putstr(ANSI_FMT("Info: Batch sequence written to image successfully.\n\r", ANSI_FG_GREEN));
    } else {
        bios_putstr(ANSI_FMT("ERROR: Failed to write batch sequence to image (code: ", ANSI_BG_RED));
        char ret_str[5];
        bios_putstr(itoa(write_ret, ret_str, 10)); // Convert error code to string to print
        bios_putstr(ANSI_FMT(").\n\r", ANSI_NONE));
    }

    return 0;
}

/**
 * @brief Command handler to execute a sequence of tasks from a batch file.
 *
 * Reads a predefined batch file from an SD card, parses the task names,
 * and launches the first task in the sequence. Subsequent tasks are
 * handled by a separate kernel-level batch handler.
 *
 * @param args Arguments passed to the command (which are ignored).
 * @return Always returns 0.
 */
int cmd_exec_batch(char *args) {
    // This command does not use arguments, so print a warning if any are provided
    if (args != NULL && *args != '\0') {
        bios_putstr(ANSI_FMT("WARNING: 'exec_batch' command does not take arguments. Ignoring.\n\r", ANSI_FG_YELLOW));
    }

    // --- 1. Read the batch file from the SD card into the global buffer ---
    int read_ret = bios_sd_read((uintptr_t)batch_sequence_buffer, BATCH_FILE_SIZE_SECTORS, g_batch_file_start_sector);

    if (read_ret != 0) { // Assuming 0 indicates success
        bios_putstr(ANSI_FMT("ERROR: Failed to read batch file from image (code: ", ANSI_BG_RED));
        char ret_str[5];
        bios_putstr(itoa(read_ret, ret_str, 10)); // Convert error code to string
        bios_putstr(ANSI_FMT(").\n\r", ANSI_NONE));
        return 0;
    }

    // --- 2. Parse the buffer content into a list of task names ---
    batch_total_tasks = parse_batch_file(batch_sequence_buffer, batch_sequence, MAX_BATCH_TASKS);

    // Check if parsing failed or if the file was empty
    if (batch_total_tasks <= 0) {
        bios_putstr(ANSI_FMT("ERROR: No valid tasks found in batch file.", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
        return 0;
    }

    // --- 3. Initialize the system's batch mode state ---
    in_batch_mode = true;
    batch_current_task_idx = 0;
    *(bool *)(IN_BATCH_MODE_LOC) = true;
    *(short *)(BATCH_TASK_INDEX_LOC) = 0;
    *(short *)(BATCH_TOTAL_TASKS_LOC) = batch_total_tasks;

    bios_putstr(ANSI_FMT("Info: Starting batch execution...\n\r", ANSI_FG_BLUE));

    // --- 4. Launch the first task in the sequence ---
    // The name is retrieved from the array we just populated
    cmd_exec(batch_sequence[batch_current_task_idx]);

    // After this function returns, the operating system's scheduler or main loop
    // will see that `in_batch_mode` is true and will use a special handler
    // to launch the next task (`batch_sequence[1]`) when the first one finishes.
 
    return 0;
}

```

>[!Note]
>The `cmd_exec_batch` function will return to kernel after it's execution. At this time, only one batch task has been processed. So, when the kernel `main` function starts, it should first check whether there's remaining batch tasks to be processed. If yes, it will call the `kernel_batch_handler` to handle the next batch task.

The implementation of `kernel_batch_handler` is as follows:

```C
/**
 * @brief Handles the continuation of batch processing when the kernel restarts.
 *
 * This function is called early in the kernel's main loop. It checks if
 * 'in_batch_mode' is true. If so, it loads the next task from the batch
 * sequence and executes it. If the batch is finished, it resets the state.
 */
void kernel_batch_handler(bool in_batch_mode, int batch_current_task_idx, int batch_total_tasks, int batch_io_buffer_val) {
    if (in_batch_mode) {
        bios_putstr(ANSI_FMT("Info: Continuing batch processing...", ANSI_BG_BLUE));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.

        // --- 1. Read and parse the batch file to repopulate the task list ---
        int read_ret = bios_sd_read((uintptr_t)batch_sequence_buffer,
                                    BATCH_FILE_SIZE_SECTORS, g_batch_file_start_sector);
        if (read_ret != 0) {
            bios_putstr(ANSI_FMT("ERROR: Failed to read batch file for continuation.", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
            // Critical error, reset batch mode and fall through to shell
            in_batch_mode = false;
            *(bool *)(IN_BATCH_MODE_LOC) = false; // Persist state change
            goto end_batch_mode_check;
        }

        int parsed_count = parse_batch_file(batch_sequence_buffer,
                                            batch_sequence, MAX_BATCH_TASKS);
        if (parsed_count <= 0) {
            bios_putstr(ANSI_FMT("ERROR: Invalid batch file content for continuation.", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
            // File is corrupt/empty, reset batch mode
            in_batch_mode = false;
            *(bool *)(IN_BATCH_MODE_LOC) = false; // Persist state change
            goto end_batch_mode_check;
        }

        // --- 2. Determine the next task to run ---
        batch_current_task_idx++; // Increment to the next task index

        if (batch_current_task_idx < batch_total_tasks) {
            // --- 3a. More tasks remain: Launch the next one ---
            bios_putstr(ANSI_FMT("Info: Launching next task in batch: ", ANSI_FG_GREEN));
            bios_putstr(batch_sequence[batch_current_task_idx]);
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));

            // Update the persistent batch state in the memory-mapped region
            *(short *)(BATCH_TASK_INDEX_LOC) = batch_current_task_idx;

            // Use inline assembly to set the a0 register. In RISC-V, a0 is used
            // for the first argument to a function and for its return value.
            // This effectively passes the previous task's output as input to the next.
            asm volatile ("mv a0, %0" : : "r" (batch_io_buffer_val));

            // Print the return value out
            char temp_buf[] = "_____";
            char *retval_buf = itoa(batch_io_buffer_val, temp_buf, 10);
            bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Got return value ", ANSI_FG_GREEN));
            bios_putstr(ANSI_FG_CYAN);
            bios_putstr(retval_buf);
            bios_putstr(ANSI_NONE);
            bios_putstr(ANSI_FMT(", passing it to 'a0'.\n", ANSI_FG_GREEN));

            // This call will not return in the traditional sense. It will jump
            // to the new task's entry point, and the kernel will be restarted
            // by the task's exit handler (crt0.S).
            cmd_exec(batch_sequence[batch_current_task_idx]);

        } else {
            // --- 3b. No more tasks: The batch has finished ---
            bios_putstr(ANSI_FMT("Info: Batch processing finished.\n\r", ANSI_FG_GREEN));
            in_batch_mode = false;

            // Reset all persistent batch state variables in the memory-mapped region
            *(bool *)(IN_BATCH_MODE_LOC) = false;
            *(short *)(BATCH_TASK_INDEX_LOC) = 0;
            *(short *)(BATCH_TOTAL_TASKS_LOC) = 0;

            // Print the return value out
            char temp_buf[] = "_____";
            char *retval_buf = itoa(batch_io_buffer_val, temp_buf, 10);
            bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Final return value: ", ANSI_FG_GREEN));
            bios_putstr(ANSI_FG_CYAN);
            bios_putstr(retval_buf);
            bios_putstr(ANSI_NONE);
            bios_putstr(ANSI_FMT(".\n", ANSI_FG_GREEN));
        }
    }

end_batch_mode_check:
    // This label is a jump target to allow the function to gracefully exit
    // batch mode on error and continue to the regular shell prompt.
    return;
}

```

The checking logic in early stage of `main` function is as follows:

```C
    // Batch mode check and handler
    bool in_batch_mode = *(bool *)(IN_BATCH_MODE_LOC);
    int batch_task_index = *(short *)(BATCH_TASK_INDEX_LOC);
    int batch_total_tasks = *(short *)(BATCH_TOTAL_TASKS_LOC);
    batch_io_buffer_val = *(uint32_t *)(BATCH_IO_BUFFER_LOC);
    kernel_batch_handler(in_batch_mode, batch_task_index, batch_total_tasks, batch_io_buffer_val);
```

### Error: `Instruction access fault , epc 0`

After implemented `cmd_batch_write`, I encountered the following error:

```
(cmd) write_batch 2048
exception code: 1 , Instruction access fault , epc 0 , ra 50201bfc
### ERROR ### Please RESET the board ###
```

I carefully reviewed the gdb output:

```
0x0000000000001000 in ?? ()
(gdb) display/5i $pc
1: x/5i $pc
=> 0x1000:      auipc   t0,0x0
   0x1004:      addi    a2,t0,40
   0x1008:      csrr    a0,mhartid
   0x100c:      ld      a1,32(t0)
   0x1010:      ld      t0,24(t0)
(gdb) b bios_sd_write
Breakpoint 1 at 0x50201be0: file include/os/kernel.h, line 23.
(gdb) c
Continuing.

Breakpoint 1, bios_sd_write (mem_address=1344289624, num_of_blocks=2, block_id=<optimized out>) at include/os/kernel.h:51
51          return call_jmptab(SD_WRITE, (long)mem_address, (long)num_of_blocks, \
1: x/5i $pc
=> 0x50201be0 <cmd_write_batch+266>:    addi    a5,a5,-224
   0x50201be4 <cmd_write_batch+270>:    ld      a5,0(a5)
   0x50201be6 <cmd_write_batch+272>:    auipc   a2,0x2
   0x50201bea <cmd_write_batch+276>:    lwu     a2,-150(a2)
   0x50201bee <cmd_write_batch+280>:    slli    a0,s3,0x20
(gdb) x/5i $pc
=> 0x50201be0 <cmd_write_batch+266>:    addi    a5,a5,-224
   0x50201be4 <cmd_write_batch+270>:    ld      a5,0(a5)
   0x50201be6 <cmd_write_batch+272>:    auipc   a2,0x2
   0x50201bea <cmd_write_batch+276>:    lwu     a2,-150(a2)
   0x50201bee <cmd_write_batch+280>:    slli    a0,s3,0x20
(gdb) p $a5
$1 = 1375731712
(gdb) p/x $a5
$2 = 0x52000000
(gdb) si
0x0000000050201be4 in call_jmptab (which=4, arg0=1344289624, arg1=2, arg2=<optimized out>, arg3=0, arg4=0) at include/os/kernel.h:23
23          return func(arg0, arg1, arg2, arg3, arg4);
1: x/5i $pc
=> 0x50201be4 <cmd_write_batch+270>:    ld      a5,0(a5)
   0x50201be6 <cmd_write_batch+272>:    auipc   a2,0x2
   0x50201bea <cmd_write_batch+276>:    lwu     a2,-150(a2)
   0x50201bee <cmd_write_batch+280>:    slli    a0,s3,0x20
   0x50201bf2 <cmd_write_batch+284>:    li      a4,0
(gdb) p/x $a5
$3 = 0x51ffff20
```

Since `0x51ffff20` is really close to the `jmptab` base address, I decided that the problem must locate in `jmptab`: the program doesn't know where it should jump, so it jump to `0`.

The code confirms my guess -- The original code did not add `bios_sd_write` to `jmptab`.

```C
static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]         = (long (*)())sd_write; <- should add this line
}
```

### Implementation User functions

We implement four new user functions as follows:

```C
#include <kernel.h>

int main() {
    int num = 5;
    return num;
}
```

```C
#include <kernel.h>
#define BOOT_LOADER_SIG_OFFSET (0x1fe + 0x50200000)
#define BATCH_IO_BUFFER_LOC (BOOT_LOADER_SIG_OFFSET - 8)

int main(void) {
    int input_val = *(short *)(BATCH_IO_BUFFER_LOC);
    return input_val + 10;
}
```

```C
#include <kernel.h>
#define BOOT_LOADER_SIG_OFFSET (0x1fe + 0x50200000)
#define BATCH_IO_BUFFER_LOC (BOOT_LOADER_SIG_OFFSET - 8)

int main(void) {
    int input_val = *(short *)(BATCH_IO_BUFFER_LOC);
    return input_val * 3;
}
```

```C
#include <kernel.h>
#define BOOT_LOADER_SIG_OFFSET (0x1fe + 0x50200000)
#define BATCH_IO_BUFFER_LOC (BOOT_LOADER_SIG_OFFSET - 8)

int main(void) {
    int input_val = *(short *)(BATCH_IO_BUFFER_LOC);
    return input_val * input_val;
}
```

> [!Warning] Return Value Passing
> Initially, I planned to pass the returned value to `a0`, however, user `main` function could not take `a0` as it's input. As a result, I choose to directly obtain the value from memory.

### Enhancing UI Appearance

Let us be retro. The first batch processing system is IBM GM-NAA I/O Batch Processing System. It is definitely exciting to print an IBM logo when the OS starts up. This is the printing function:

Source: [AbhishekGhosh/IBM-ASCII-Logo-For-SSH: IBM ASCII Logo For SSH ASCII Logo for SSH Pre-Login](https://github.com/AbhishekGhosh/IBM-ASCII-Logo-For-SSH)

```C
/**
 * @brief Print the batch processing system logo when OS starts.
 */
void print_logo(void) {
    bios_putstr("=====================================================\n\r");
    bios_putstr("        GM-NAA I/O BATCH PROCESSING MONITOR\n\r");
    bios_putstr("=====================================================\n\r");
    bios_putstr("\n\r");
    bios_putstr("*****************************************************\n\r");
    bios_putstr("*****************************************************\n\r");
    bios_putstr("*****************************************************\n\r");
    bios_putstr("███████████  ████████████      ████████      ████████\n\r");
    bios_putstr("███████████  ███████████████   █████████    █████████\n\r");
    bios_putstr("   █████        ████   █████     ████████  ████████\n\r");
    bios_putstr("   █████        ███████████      ████  ███ ███ ████\n\r");
    bios_putstr("   █████        ███████████      ████  ███████ ████\n\r");
    bios_putstr("   █████        ████   █████     ████   █████  ████\n\r");
    bios_putstr("███████████  ███████████████   ██████    ███   ██████\n\r");
    bios_putstr("███████████  ████████████      ██████     █    ██████\n\r");
    bios_putstr("*****************************************************\n\r");
    bios_putstr("*  This system is for the use of authorized users   *\n\r");
    bios_putstr("*  only. Usage of  this system may be monitored     *\n\r");
    bios_putstr("*  and recorded                                     *\n\r");
    bios_putstr("*****************************************************\n\r");
    bios_putstr("\n\r"); // Add an extra newline for spacing
}
```

To ensure the logo is only printed once, we can add a flag in a specific mem address, and in kernel `main` function, we can use this logic to check for it's status:

```C
    // Print logo on startup
    if (*(short *)(LOGO_HAS_PRINTED) == 0) {
        print_logo();
        *(short *)(LOGO_HAS_PRINTED) = 1;
    }
```

### Testing the Batch Processing System

This is a full process of how this batch processing system would work:

```
=> loadboot
It's Chuxiao Han's bootloader...
DEBUG: task detected, '2048'
DEBUG: task detected, 'app_num1'
DEBUG: task detected, 'app_num2'
DEBUG: task detected, 'app_num3'
DEBUG: task detected, 'app_num4'
DEBUG: task detected, 'auipc'
DEBUG: task detected, 'bss'
DEBUG: task detected, 'data'
Info: Hello OS!
Info: bss check: t version: 2
Info: Loaded 8 tasks.
=====================================================
        GM-NAA I/O BATCH PROCESSING MONITOR
=====================================================

*****************************************************
*****************************************************
*****************************************************
███████████  ████████████      ████████      ████████
███████████  ███████████████   █████████    █████████
   █████        ████   █████     ████████  ████████
   █████        ███████████      ████  ███ ███ ████
   █████        ███████████      ████  ███████ ████
   █████        ████   █████     ████   █████  ████
███████████  ███████████████   ██████    ███   ██████
███████████  ████████████      ██████     █    ██████
*****************************************************
*  This system is for the use of authorized users   *
*  only. Usage of  this system may be monitored     *
*  and recorded                                     *
*****************************************************

(cmd) write_batch app_num1 app_num2 app_num3 app_num4
Info: Batch sequence written to image successfully.
(cmd) exec_batch
Info: Starting batch execution...
Info: Now executing task 1, app_num1
Info: Windows is loading files...
DEBUG: Loaded 'app_num1'. First bytes in memory:
  17 05 00 00 13 05 85 08 97 05 00 00 93 85 05 08
  Last bytes in memory:
  18 00 00 00 c6 ff ff ff 04 00 00 00 00 00 00 00
Info: Starting task...
DEBUG: task detected, '2048'
DEBUG: task detected, 'app_num1'
DEBUG: task detected, 'app_num2'
DEBUG: task detected, 'app_num3'
DEBUG: task detected, 'app_num4'
DEBUG: task detected, 'auipc'
DEBUG: task detected, 'bss'
DEBUG: task detected, 'data'
Info: Hello OS!
Info: bss check: t version: 2
Info: Continuing batch processing...
Info: Launching next task in batch: app_num2
Info: Got return value 5, passing it to 'a0'.
Info: Now executing task 2, app_num2
Info: Windows is loading files...
DEBUG: Loaded 'app_num2'. First bytes in memory:
  17 05 00 00 13 05 05 09 97 05 00 00 93 85 85 08
  Last bytes in memory:
  18 00 00 00 be ff ff ff 0c 00 00 00 00 00 00 00
Info: Starting task...
DEBUG: task detected, '2048'
DEBUG: task detected, 'app_num1'
DEBUG: task detected, 'app_num2'
DEBUG: task detected, 'app_num3'
DEBUG: task detected, 'app_num4'
DEBUG: task detected, 'auipc'
DEBUG: task detected, 'bss'
DEBUG: task detected, 'data'
Info: Hello OS!
Info: bss check: t version: 2
Info: Continuing batch processing...
Info: Launching next task in batch: app_num3
Info: Got return value 15, passing it to 'a0'.
Info: Now executing task 3, app_num3
Info: Windows is loading files...
DEBUG: Loaded 'app_num3'. First bytes in memory:
  17 05 00 00 13 05 85 09 97 05 00 00 93 85 05 09
  Last bytes in memory:
  18 00 00 00 b6 ff ff ff 14 00 00 00 00 00 00 00
Info: Starting task...
DEBUG: task detected, '2048'
DEBUG: task detected, 'app_num1'
DEBUG: task detected, 'app_num2'
DEBUG: task detected, 'app_num3'
DEBUG: task detected, 'app_num4'
DEBUG: task detected, 'auipc'
DEBUG: task detected, 'bss'
DEBUG: task detected, 'data'
Info: Hello OS!
Info: bss check: t version: 2
Info: Continuing batch processing...
Info: Launching next task in batch: app_num4
Info: Got return value 45, passing it to 'a0'.
Info: Now executing task 4, app_num4
Info: Windows is loading files...
DEBUG: Loaded 'app_num4'. First bytes in memory:
  17 05 00 00 13 05 05 09 97 05 00 00 93 85 85 08
  Last bytes in memory:
  18 00 00 00 be ff ff ff 0e 00 00 00 00 00 00 00
Info: Starting task...
DEBUG: task detected, '2048'
DEBUG: task detected, 'app_num1'
DEBUG: task detected, 'app_num2'
DEBUG: task detected, 'app_num3'
DEBUG: task detected, 'app_num4'
DEBUG: task detected, 'auipc'
DEBUG: task detected, 'bss'
DEBUG: task detected, 'data'
Info: Hello OS!
Info: bss check: t version: 2
Info: Continuing batch processing...
Info: Batch processing finished.
Info: Final return value: 2025.
Info: Loaded 8 tasks.
(cmd) ls
Info: Listing tasks:
  [0] 2048
  [1] app_num1
  [2] app_num2
  [3] app_num3
  [4] app_num4
  [5] auipc
  [6] bss
  [7] data
```
