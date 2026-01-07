# Project 6: File System Implementation

**University of Chinese Academy of Sciences - Operating System (RISC-V)**

## 1. Overview
This project implements a simple yet robust physical file system for the UCAS-OS kernel on RISC-V. It supports multi-level directory structures, fundamental file I/O operations (read, write, open, close), hard links, and advanced caching mechanisms with configurable write policies. The goal is to provide a persistent storage layer that integrates seamlessly with the kernel's process management and virtual memory subsystems.

## 2. Implemented Features

### 2.1 File System Structure & Initialization (Task 1)
*   **On-Disk Layout:** Adheres to a Unix-like structure: Superblock, Block Bitmap, Inode Bitmap, Inode Table, and Data Blocks. The file system starts at a 512MB offset on the SD card to avoid conflicts with the kernel image.
*   **Superblock (`superblock_t`):** Stores critical metadata including magic number, file system size, and the layout of all other regions.
*   **Inodes (`inode_t`):** Describes files and directories, holding metadata such as type (regular file/directory), size, link count, and block pointers.
    *   **Multi-level Indexing:** Supports large files (demonstrated up to 128MB, scalable to GBs) using direct, single-indirect, and double-indirect block pointers.
*   **Directory Entries (`dentry_t`):** Maps filenames to inode numbers within directory data blocks.
*   **Initialization (`mkfs`):** The `mkfs` command (or automatic kernel initialization) formats the disk, sets up all metadata structures, and creates the root directory and special `/proc/sys/vm` configuration file.
*   **Status (`statfs`):** Provides detailed information about the file system's current state, including usage statistics.

### 2.2 File and Directory Operations (Task 1 & 2, A-core Extensions)
*   **Directory Management:**
    *   `cd <path>`: Changes the current working directory for a process. Each process (`pcb_t`) maintains its `cwd_ino`.
    *   `ls [path]`: Lists the contents of a directory.
    *   `ls -l [path]`: (A-core) Provides a detailed listing, showing inode mode, hard links, size, and inode number.
    *   `mkdir <path>`: Creates a new directory. Supports multi-level paths.
    *   `rmdir <path>`: Removes an empty directory. Supports multi-level paths.
*   **File I/O:**
    *   **File Descriptors (`fdesc_t`):** Kernel-side structures representing open files, tracking the inode, access mode, and current read/write pointers.
    *   `open <path> <mode>`: Opens an existing file or creates a new one if it doesn't exist and the mode allows. Returns a file descriptor.
    *   `read <fd> <buffer> <length>`: Reads data from a file into a buffer.
    *   `write <fd> <buffer> <length>`: Writes data from a buffer into a file.
    *   `close <fd>`: Closes a file descriptor.
    *   `lseek <fd> <offset> <whence>`: (A-core) Repositions the read/write pointer within a file using `SEEK_SET`, `SEEK_CUR`, or `SEEK_END`. Essential for handling large files and sparse writes.
*   **File Manipulation (A-core Extensions):**
    *   `ln <src> <dst>`: Creates a hard link, adding another directory entry pointing to an existing file's inode and incrementing its link count.
    *   `rm <path>`: Removes a file or an empty directory. Decrements the inode's link count and frees associated data blocks and the inode itself if the link count drops to zero.
    *   `touch <file>`: Creates an empty file.
    *   `cat <file>`: Prints the content of a file to the console.

### 2.3 File System Caching (Task 3 C-core)
*   **Page Cache (Block Cache):**
    *   A kernel-resident cache for data blocks (4KB units) to minimize slow SD card access.
    *   Employs an LRU (Least Recently Used) replacement policy for efficient cache management.
    *   `sd_read_block()` and `sd_write_block()` transparently interact with this cache.
*   **Dentry Cache (Metadata Cache):**
    *   Caches `(parent_ino, name) -> ino` mappings to accelerate path resolution, reducing disk lookups for directory entries.
    *   Uses a simple hashing scheme for fast lookup.
*   **Configurable Write Policies:**
    *   **Write-Back:** (Default) Data is written to cache, marked dirty, and flushed to SD card periodically. Offers high performance, but risks data loss on crashes.
    *   **Write-Through:** Data is written to cache AND immediately to SD card. Ensures higher data reliability at the cost of write performance.
*   **Dynamic Configuration (`/proc/sys/vm`):**
    *   Cache parameters (`page_cache_policy`, `write_back_freq`, `dcache_enable`, `clear_cache`) are controlled via a virtual file `/proc/sys/vm`.
    *   Changes to `/proc/sys/vm` are applied dynamically by the `do_fs_sync()` syscall. Switching from Write-Back to Write-Through triggers an immediate flush of all dirty blocks.
*   **Filesystem Daemon (`fs_daemon`):**
    *   A kernel process launched at startup that periodically calls `sys_fs_sync()` to flush dirty blocks from the page cache when the Write-Back policy is active, balancing performance with data persistence.

## 3. How to Build and Run

### Prerequisites
*   RISC-V Toolchain (gcc, gdb, qemu)
*   `tap0` network interface configured on the host machine (if testing network features from previous projects).

### Compilation
```bash
make clean
make
```
**Note:** The `Makefile` includes a `dd` command to pad the generated image to 1GB. This is crucial for QEMU testing to allow the 512MB file system to operate without exceeding the virtual disk size.

### Running
```bash
make run      # Run in QEMU
make debug    # Run in QEMU with GDB attached
```

## 4. Test Guide

Once the OS boots and you enter the shell (or user programs run):

### 1. File System Initialization & Status
```bash
mkfs           # Initialize/format the file system
statfs         # Display superblock information and usage statistics
```
*Observation:* `mkfs` should print initialization details, and `statfs` should show the file system layout and (initially empty) usage.

### 2. Directory Operations
```bash
mkdir /usr
mkdir /usr/local
cd /usr/local
ls             # Should show empty
cd /
ls -l /usr     # Should show 'local' directory with details
rmdir /usr/local # Should succeed if empty
rmdir /usr     # Should fail if not empty, then succeed after removing /usr/local
```
*Observation:* Verify directory creation, navigation, listing, and removal. `ls -l` should provide detailed inode information.

### 3. Basic File I/O
```bash
touch my_file.txt
cat my_file.txt # Should be empty
exec rwfile     # Runs a test that writes "hello world!" * 10 to 1.txt, then reads and prints
cat 1.txt       # Verify content persistence after rwfile finishes
```
*Observation:* `touch` creates a file, `cat` displays its content. `rwfile` demonstrates `open`, `write`, `read`, `close`. Content should persist.

### 4. Hard Links
```bash
ln 1.txt link_to_1.txt
ls -l           # Check link count for 1.txt and link_to_1.txt
rm 1.txt        # Remove original file
cat link_to_1.txt # Content should still be accessible via link
```
*Observation:* Hard links allow multiple names to refer to the same file. `rm` only deletes the inode and its data when the last link is removed.

### 5. Large File Support & Sparse Writes
```bash
open large.bin 3 # Use sys_open directly or a user program that calls it
exec largefile   # This program writes to offset 0, 1MB, and 128MB in large.bin
                 # and then verifies the written content. It uses sys_lseek internally.
```
*Observation:* `largefile` demonstrates `lseek` and the file system's ability to handle sparse files and indirect block indexing for large offsets.

### 6. File System Caching
```bash
exec cache_test  # Runs various tests for the caching subsystem
```
*Observation:* `cache_test` will output performance differences for:
*   Cold vs. Warm reads (data cache).
*   Dcache Enabled vs. Disabled (metadata cache).
*   Write-Through vs. Write-Back write performance.
It also demonstrates dynamic policy changes via `/proc/sys/vm`.

### 7. Interactive Text Editor
```bash
exec vim my_document.txt # Launch a simple interactive text editor
```
*Observation:* A basic `vim`-like editor allowing normal, insert, and command modes (:w, :q) to create and edit files interactively.

## 5. Key Design Decisions

### 5.1 Unix-like File System Abstraction
The design closely mimics a simplified Unix-like file system, providing familiar abstractions (superblock, inode, dentry, file descriptors) to users and internal kernel components. This modular approach enhances maintainability and extensibility.

### 5.2 Indirect Block Indexing for Scalability
The `inode_t` structure utilizes direct, single-indirect, and double-indirect block pointers. This design choice is fundamental for efficiently supporting files ranging from small (direct blocks) to very large (gigabytes via indirect blocks) without wasting inode space for every potential block.

### 5.3 Layered I/O with Caching
The file system employs a layered I/O approach. `sd_read_block()` and `sd_write_block()` abstract raw SD card access and integrate directly with a kernel-level page cache. This cache, along with a dedicated dentry cache, significantly reduces disk I/O, boosting overall file system performance.

### 5.4 Configurable and Persistent Caching Policies
The dynamic configuration of write-back and write-through policies via the `/proc/sys/vm` interface allows administrators to tune the file system for either performance (write-back) or reliability (write-through) at runtime. The `fs_daemon` ensures that the write-back policy flushes dirty data persistently.

### 5.5 Per-Process Current Working Directory
Each `pcb_t` (Process Control Block) now includes a `cwd_ino` field, enabling each process to have its own current working directory. This is essential for standard shell behavior and enhances process isolation.