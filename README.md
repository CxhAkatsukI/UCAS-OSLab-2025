# RISC-V OS Experiment - Project 0: Prime Number Check

## Overview
This project implements a RISC-V assembly program to identify prime numbers within the range of 1 to 200. It utilizes a dedicated function (`is_prime`) to determine the primality of each number and stores the boolean results (0 for not prime, 1 for prime) in a specific memory region.

## Files
- `prime_check.S`: The main RISC-V assembly source code containing the `main` function and the `is_prime` function.
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
4.  **Inspect memory** to verify results (after the program halts at `j .`):
    ```gdb
    x/200tb 0x50200002
    ```
    This command will display 200 bytes in binary format, starting from memory address `0x50200002`. Each byte will represent the primality (0 or 1) of a number from 2 to 201.

## Verification
The program stores 0 (not prime) or 1 (prime) for each number from 2 to 201. You can inspect the memory at `0x50200002` using GDB to see the sequence of results. For example, `x/200xb 0x50200002` will show the hex values.
