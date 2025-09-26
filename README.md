# RISC-V OS Experiment - Project 0: Sum 1 to 50

## Overview
This project implements a simple RISC-V assembly program to calculate the sum of integers from 1 to 50. The final sum is stored in a specific memory location, which can be verified using a debugger.

## Files
- `test.S`: The main RISC-V assembly source code containing the sum calculation logic.
- `Makefile`: Build script for compiling the assembly code and running/debugging with QEMU.
- `riscv.lds`: Linker script for memory layout.
- `.gitignore`: Specifies files to be ignored by Git (e.g., compiled executables, cache files).

## How to Build
To compile the assembly code into an executable:
```bash
make
```

## How to Run (QEMU)
To run the compiled program in the QEMU emulator:
```bash
make run
```
(Exit QEMU by pressing `Ctrl+A` then `x`)

## How to Debug (QEMU + GDB)
1.  **Start QEMU in debug mode** (in a separate terminal):
    ```bash
    make debug
    ```
2.  **Start GDB and connect to QEMU** (in your current terminal):
    ```bash
    make gdb
    ```
3.  **Continue execution** in GDB:
    ```gdb
    c
    ```
4.  **Inspect memory** to verify the result (after the program halts at `j .`):
    ```gdb
    x/d 0x50200000
    ```
    This command will display the 64-bit integer value stored at memory address `0x50200000` in decimal format.

## Verification
The sum of integers from 1 to 50 is 1275. You can verify this by inspecting the memory at `0x50200000` using GDB, which should show the value `1275`.
