## RISC-V OS Experiment - Project 1

This branch fulfills the requirements up to Task 5 (C-Core) for project 1 as describe in the lecture notes and guidebook.

### Task 1: Simple Bootloader
*   Developed a basic bootloader (`arch/riscv/boot/bootblock.S`) capable of printing a custom welcome message to the console using BIOS functions. This demonstrates understanding of the boot process and assembly-level BIOS calls.

### Task 2: Kernel Loading and Initialization
*   Modified the bootloader (`bootblock.S`) to load the kernel from the SD card into a predefined memory location.
*   Implemented kernel entry point (`arch/riscv/kernel/head.S`) to clear the BSS segment and set up the stack pointer for the kernel's C environment.
*   Initialized the kernel's main function (`init/main.c`) to print a "Hello OS!" message and echo characters from the console.

### Task 3 (Integrated into Task 4): Application Loading
*   Developed the `createimage` tool (`tools/createimage.c`) to combine the bootloader, kernel, and user applications into a single image file.
*   Implemented the `loader` (`kernel/loader/loader.c`) to load user applications into memory.
*   Implemented the C runtime (`arch/riscv/crt0/crt0.S`) for user applications, including BSS clearing and stack setup.

### Task 4: Tight Packing and Application Loading by Name
*   **Tight Packing:** Modified `createimage.c` to store the kernel and user applications in the image file using their actual sizes, without fixed-size padding between them (only the bootblock is padded to one sector). This optimizes image space utilization.
*   **Application Metadata:** Extended `task_info_t` to store each application's name, actual byte offset, byte size, start sector, and total sectors required. This metadata is stored in the first sector of the image.
*   **Static Loading:** The kernel's `loader` (`kernel/loader/loader.c`) now uses this precise metadata to load applications from the image into their fixed memory regions.
*   **Launch by Name:** The kernel's command-line interface (`init/cmd.c`) allows users to launch applications by typing their names (e.g., `exec 2048`).

### Task 5 (C-Core): Batch Processing System
*   **Command-Line Interface (CLI):** Implemented a robust CLI in `init/cmd.c` with custom tokenization, supporting commands:
    *   `help`: Displays available commands.
    *   `ls`: Lists all loaded user applications.
    *   `exec <task_name_or_id>`: Executes a single user application.
    *   `write_batch <task1> <task2> ...`: Writes a sequence of task names to a persistent batch file within the image.
    *   `exec_batch`: Executes the stored batch processing sequence.
*   **Persistent Batch State:** Batch processing state (e.g., `in_batch_mode`, `current_task_index`, `io_buffer`) is stored in fixed locations within the first sector of the image, making it persistent across kernel restarts.
*   **Sequential Execution:** The kernel automatically continues batch processing after each application finishes, leveraging a kernel restart mechanism.
*   **Input/Output Passing:**
    *   `crt0.S` saves the return value of an application (in `a0`) to a designated `batch_io_buffer` in the first sector.
    *   The kernel's batch handler reads this value and sets input calue for the next application, effectively passing output as input.
*   **Number Processing Applications:** Four custom user applications (`app_num1.c`, `app_num2.c`, `app_num3.c`, `app_num4.c`) are provided to demonstrate the input/output passing mechanism:
    1.  `app_num1`: Outputs an initial number.
    2.  `app_num2`: Receives input, adds 10, outputs result.
    3.  `app_num3`: Receives input, multiplies by 3, outputs result.
    4.  `app_num4`: Receives input, squares the result, outputs result.

## Memory Layout

The image file is structured as follows:
1.  **Sector 0 (Bootblock):** Contains the bootloader code and critical metadata (kernel size, task information array, batch file start sector, batch state variables).
2.  **Kernel:** Immediately follows the bootblock, tightly packed.
3.  **User Applications:** Follow the kernel, each tightly packed.
4.  **Batch File:** Located immediately after the last user application, reserved for storing batch sequences.

## Build Instructions

To build the project, navigate to the project root directory and use the following `make` commands:

*   `make clean`: Cleans up all generated build files and the `build/` directory.
*   `make all`: Performs a full build, including:
    *   Compiling the bootloader.
    *   Compiling the kernel (including `main.c`, `cmd.c`, `loader.c`, etc.).
    *   Compiling the user applications (e.g., `2048`, `auipc`, `bss`, `data`, `app_num1` to `app_num4`).
    *   Compiling the `createimage` tool.
    *   Generating the final `build/image` file.
*   `make image`: Only generates the `build/image` file using the `createimage` tool.
*   `make run`: Starts QEMU with the generated `build/image`.
*   `make debug`: Starts QEMU in debug mode, waiting for a GDB connection.

## Usage Instructions

1.  **Build the project:**
    ```bash
    make all
    ```
2.  **Run QEMU:**
    ```bash
    make run
    ```
3.  **In the QEMU console:**
    *   Type `loadboot` and press Enter.
    *   The kernel will boot, print "Hello OS!", and present a `(cmd)` prompt.

### Available Commands:

*   **`help`**: Displays a list of all supported commands and their descriptions.
*   **`ls`**: Lists all currently loaded user applications by their index and name.
*   **`exec <task_name_or_id>`**: Executes a specific user application.
    *   Example: `exec 2048` or `exec 0` (if 2048 is task 0).
*   **`write_batch <task1_name> <task2_name> ...`**: Writes a batch sequence to the image.
    *   Example: `write_batch app_num1 app_num2 app_num3 app_num4`
*   **`exec_batch`**: Executes the batch sequence previously written to the image.

### Example Batch Processing Workflow:

1.  **List available applications:**
    ```shell
    (cmd) ls
    Info: Listing tasks:
      [0] 2048
      [1] auipc
      [2] bss
      [3] data
      [4] app_num1
      [5] app_num2
      [6] app_num3
      [7] app_num4
    ```
2.  **Write a batch sequence:**
    ```shell
    (cmd) write_batch app_num1 app_num2 app_num3 app_num4
    Info: Batch sequence written to image successfully.
    ```
3.  **Execute the batch:**
    ```shell
```
```
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
    (cmd)
```
```

## Project Structure

*   `arch/riscv/`: RISC-V specific assembly and BIOS code (`bootblock.S`, `head.S`, `crt0.S`, `common.c`).
*   `build/`: Generated build artifacts (ELF files, `image`).
*   `include/`: Kernel-wide header files (`os/kernel.h`, `os/task.h`, `os/string.h`, `cmd.h`, `type.h`).
*   `init/`: Kernel initialization code (`main.c`, `cmd.c`).
*   `kernel/loader/`: Application loader (`loader.c`).
*   `libs/`: Utility functions (`string.c`).
*   `test/test_project1/`: User application source files (`2048.c`, `auipc.c`, `bss.c`, `data.c`, `app_num1.c` to `app_num4.c`).
*   `tools/`: Host-side tools (`createimage.c`).
*   `Makefile`: Project build system.
*   `riscv.lds`: Linker script.

## Challenges Encountered & Solutions & Development Notes

See `development_note.md`.

## References

*   Lecture 1 Bootloader (PowerPoint)
*   Operating Systems Project 1 – Bootloader Guidebook
