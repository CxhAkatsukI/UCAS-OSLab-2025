# Development Note

### Implementing File System Initialization and Core Structures

To meet the requirement of Task 1 in the guidebook, which involves designing and implementing a physical file system, we first define the fundamental data structures that describe the file system's layout and content. These structures are crucial for organizing data on the storage medium (SD card).

#### 1. Core File System Data Structures (`include/os/fs.h`)

The `include/os/fs.h` header file defines the essential data structures for the superblock, directory entries, inodes, and file descriptors.

##### `superblock_t` (Superblock)

The `superblock_t` structure holds critical metadata about the entire file system. It's the first block read when mounting the file system and dictates the layout of other metadata.

```c
typedef struct superblock {
    uint32_t magic;           /* A magic number to identify the file system */
    uint32_t fs_start_sector; /* Starting sector of the file system on disk */
    uint32_t size_sectors;    /* Total size of FS in sectors */
    
    uint32_t block_map_offset; /* Offset in blocks from start of FS to block bitmap */
    uint32_t block_map_count;  /* Number of blocks occupied by block map */

    uint32_t inode_map_offset; /* Offset in blocks from start of FS to inode bitmap */
    uint32_t inode_map_count;  /* Number of blocks occupied by inode map */

    uint32_t inode_table_offset; /* Offset in blocks from start of FS to inode table */
    uint32_t inode_table_count;  /* Number of blocks occupied by inode table */

    uint32_t data_offset;      /* Offset in blocks from start of FS to data region */
    uint32_t data_count;       /* Number of data blocks */

    uint32_t inode_count;      /* Total number of inodes */
    uint32_t block_count;      /* Total number of blocks */

    uint32_t root_ino;         /* Inode number of the root directory */
} superblock_t;
```
-   **Filesystem Concept:** The superblock acts as the "table of contents" for the entire file system. It informs the kernel where to find other crucial components like the block and inode bitmaps, and the inode table. The `magic` number is used to verify that the block being read is indeed a valid superblock for this file system type. `FS_START_SECTOR` (defined as `1048576`, corresponding to 512MB) specifies the starting point of our file system on the underlying storage.

##### `dentry_t` (Directory Entry)

Directory entries are stored within data blocks belonging to a directory. Each `dentry_t` maps a filename to its corresponding inode number.

```c
typedef struct dentry {
    char name[MAX_FILE_NAME]; /* Name of the file or directory */
    uint32_t ino;             /* Inode number of the associated file/directory */
    uint32_t pad;             /* Padding for alignment or future use */
} dentry_t;
```
-   **Filesystem Concept:** Directories are essentially files that contain a list of these `dentry_t` structures. When listing the contents of a directory (`ls`), the kernel reads these entries to display filenames and then uses the `ino` to retrieve more detailed information from the inode table.

##### `inode_t` (Inode)

The `inode_t` structure describes a single file or directory. It holds metadata and pointers to the data blocks that store the actual file content.

```c
#define NDIRECT 12 /* Number of direct pointers in inode */
#define INDIRECT_BLOCK_COUNT (BLOCK_SIZE / sizeof(uint32_t)) /* Pointers per indirect block */

typedef struct inode {
    uint32_t ino;          /* Inode number (unique identifier) */
    uint32_t mode;         /* File type (IM_REG for regular, IM_DIR for directory) */
    uint32_t access;       /* Permissions (rwx - not fully implemented in this example) */
    uint32_t nlinks;       /* Number of hard links pointing to this inode */
    uint32_t size;         /* File size in bytes */
    uint32_t direct_ptrs[NDIRECT]; /* Pointers to direct data blocks */
    uint32_t indirect_ptr[3];      /* 0: single, 1: double, 2: triple indirect */
    uint32_t pad[12];       /* Padding to align or reserve space */
} inode_t;
```
-   **Filesystem Concepts:**
    -   **Inode Number (`ino`):** A unique identifier for a file or directory within the file system.
    -   **File Mode (`mode`):** Differentiates between regular files (`IM_REG`) and directories (`IM_DIR`).
    -   **Hard Links (`nlinks`):** The count of directory entries that point to this inode. When `nlinks` drops to zero, the inode and its data blocks can be freed.
    -   **Data Block Indexing:** To support large files efficiently, `inode_t` uses a multi-level indexing scheme:
        -   **Direct Pointers (`direct_ptrs`):** `NDIRECT` (12) pointers directly point to data blocks. This is efficient for small files (up to `12 * BLOCK_SIZE = 48KB`).
        -   **Single Indirect Pointer (`indirect_ptr[0]`):** Points to a data block that, in turn, contains `INDIRECT_BLOCK_COUNT` (1024) pointers to other data blocks. This extends file size significantly (up to `48KB + 1024 * BLOCK_SIZE = 4MB`).
        -   **Double Indirect Pointer (`indirect_ptr[1]`):** Points to a block that contains pointers to *single indirect blocks*. This allows for very large files (up to `4MB + 1024 * 1024 * BLOCK_SIZE = 4GB`).
        -   The `get_block_addr` function (in `kernel/fs/fs.c`) handles the logic for traversing these pointer levels.

##### `fdesc_t` (File Descriptor)

The `fdesc_t` structure represents an open file within the kernel for a specific process.

```c
typedef struct fdesc {
    uint32_t ino;          /* The inode number this fd points to */
    uint32_t read_ptr;     /* Current read offset */
    uint32_t write_ptr;    /* Current write offset */
    uint32_t access;       /* Access mode (O_RDONLY, O_WRONLY, O_RDWR) */
    uint32_t is_used;      /* Flag indicating if this descriptor is in use */
} fdesc_t;
```
-   **Filesystem Concept:** When a user program calls `open()`, the kernel finds the file's inode and then allocates an `fdesc_t` entry. This `fdesc_t` acts as the kernel's internal representation of the open file instance, managing its current read/write position and access permissions. The integer returned to the user program by `open()` is an index into an array of these `fdesc_t` structures.

#### 2. File System Initialization (`kernel/fs/fs.c` and `init/main.c`)

The initialization of the file system involves setting up the superblock, bitmaps, and the root directory.

##### `do_mkfs()` Function

The `do_mkfs()` function is responsible for formatting the file system. It writes the initial metadata structures to the SD card, effectively creating an empty, usable file system.

```c
int do_mkfs(void)
{
    /* Invalidate Page Cache */
    spin_lock_acquire(&cache_lock);
    for (int i = 0; i < CACHE_SIZE; i++) {
        page_cache[i].valid = 0;
        page_cache[i].dirty = 0; 
    }
    spin_lock_release(&cache_lock);

    /* Invalidate Directory Entry Cache (Dcache) */
    for (int i = 0; i < DCACHE_SIZE; i++) {
        dcache[i].valid = 0;
    }

    // Define file system size and calculate layout parameters
    uint32_t fs_size_bytes = 512 * 1024 * 1024; // 512MB
    uint32_t fs_size_blocks = fs_size_bytes / BLOCK_SIZE;
    uint32_t fs_size_sectors = fs_size_bytes / SECTOR_SIZE;

    // Calculate sizes for metadata regions
    uint32_t block_map_size = 4; // 4 blocks for block bitmap (for 512MB FS with 4KB blocks)
    uint32_t inode_map_size = 1; // 1 block for inode bitmap (for 4096 inodes)
    uint32_t inode_table_size = (4096 * sizeof(inode_t) + BLOCK_SIZE - 1) / BLOCK_SIZE; 

    // Calculate block offsets for each region
    uint32_t sb_block = 0;
    uint32_t bmap_block = sb_block + 1;
    uint32_t imap_block = bmap_block + block_map_size;
    uint32_t itable_block = imap_block + inode_map_size;
    uint32_t data_block = itable_block + inode_table_size;

    // Initialize superblock structure
    memset(&superblock, 0, sizeof(superblock_t));
    superblock.magic = SUPERBLOCK_MAGIC;
    superblock.fs_start_sector = FS_START_SEC;
    superblock.size_sectors = fs_size_sectors;
    superblock.block_map_offset = bmap_block;
    superblock.block_map_count = block_map_size;
    superblock.inode_map_offset = imap_block;
    superblock.inode_map_count = inode_map_size;
    superblock.inode_table_offset = itable_block;
    superblock.inode_table_count = inode_table_size;
    superblock.data_offset = data_block;
    superblock.data_count = fs_size_blocks - data_block;
    superblock.inode_count = 4096; // Max number of inodes
    superblock.block_count = fs_size_blocks; // Total blocks in FS
    superblock.root_ino = 1; // Root directory is inode 1

    // Clear all metadata and data blocks up to the data region start
    clear_blocks(0, data_block); 
    
    // Write the initialized superblock to disk (block 0)
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    memcpy((uint8_t *)buf, (uint8_t *)&superblock, sizeof(superblock_t));
    sd_write_block(0, buf);

    // Mark root inode (ino 1) as used in the inode bitmap
    memset(buf, 0, BLOCK_SIZE);
    buf[0] |= (1 << 1); // bit 1 set for inode 1
    sd_write_block(imap_block, buf);

    // Mark superblock block (block 0) and root data block (block 1) as used in the block bitmap
    memset(buf, 0, BLOCK_SIZE);
    buf[0] |= (1 << 0) | (1 << 1); // bits 0 and 1 set
    sd_write_block(bmap_block, buf);

    // Initialize the root directory inode (ino 1)
    inode_t root;
    memset(&root, 0, sizeof(inode_t));
    root.ino = 1; 
    root.mode = IM_DIR; 
    root.nlinks = 2; // . and .. links
    root.size = 2 * sizeof(dentry_t); // for . and ..
    root.direct_ptrs[0] = 1; // Root's data is in data block 1
    set_inode(1, &root);

    // Create . and .. entries for the root directory in its data block
    memset(buf, 0, BLOCK_SIZE);
    dentry_t *de = (dentry_t *)buf;
    strcpy(de[0].name, "."); de[0].ino = 1; // . points to itself
    strcpy(de[1].name, ".."); de[1].ino = 1; // .. points to itself (root's parent is root)
    sd_write_block(data_block + 1, buf); // Write to data block 1 (offset by superblock.data_offset)

    // Create /proc and /proc/sys directories, and /proc/sys/vm file
    do_mkdir("/proc");
    do_mkdir("/proc/sys");
    int fd = do_open("/proc/sys/vm", O_RDWR);
    if (fd >= 0) {
        char *content = "page_cache_policy = 1\nwrite_back_freq = 30\n";
        do_write(fd, content, strlen(content));
        do_close(fd);
    }

    return 0;
}
```
-   **Detailed Explanation:**
    1.  **Cache Invalidation:** Before formatting, both the page cache and dentry cache are cleared to ensure a clean state.
    2.  **File System Sizing:** The file system is designed for a total size of 512MB (`fs_size_bytes`), and the number of blocks and sectors are calculated based on `BLOCK_SIZE` (4KB) and `SECTOR_SIZE` (512B).
    3.  **Layout Calculation:** The function meticulously calculates the size and starting block offset for each metadata region: superblock, block bitmap, inode bitmap, inode table, and the main data region.
    4.  **Superblock Initialization:** The `superblock_t` structure is filled with these calculated parameters and a magic number (`SUPERBLOCK_MAGIC`). This superblock is then written to the very first block of the file system (block 0).
    5.  **Bitmap Setup:** The inode bitmap (inode 1 for root) and the block bitmap (block 0 for superblock, block 1 for root directory data) are initialized to mark the allocated entries.
    6.  **Root Directory Creation:**
        *   An inode for the root directory (inode number 1) is allocated and initialized with `IM_DIR` mode, `nlinks = 2` (for `.` and `..`), and its initial data block pointer set to `1`.
        *   The root directory's data block (data block 1) is populated with two `dentry_t` entries: `.` (pointing to inode 1 itself) and `..` (also pointing to inode 1, as the root has no parent).
    7.  **`/proc` Setup:** The function then proceeds to create the `/proc` and `/proc/sys` directories, and a special file `/proc/sys/vm`. This `vm` file is crucial for configuring the file system's cache policy (`page_cache_policy`) and write-back frequency (`write_back_freq`), a feature detailed further in Task 3 (C-core).

##### `init_fs()` Function (`kernel/fs/fs.c`)

The `init_fs()` function is the primary entry point for initializing the file system at kernel boot.

```c
void init_fs(void)
{
    char buf[BLOCK_SIZE];
    sd_read_block(0, buf); // Attempt to read the superblock
    memcpy((uint8_t *)&superblock, (uint8_t *)buf, sizeof(superblock_t));

    // Check if the superblock's magic number is valid
    if (superblock.magic != SUPERBLOCK_MAGIC) {
        // If not valid, the file system needs to be formatted
        do_mkfs();
        // Re-read the superblock after formatting
        sd_read_block(0, buf);
        memcpy((uint8_t *)&superblock, (uint8_t *)buf, sizeof(superblock_t));
    }

    // Initialize all file descriptors as unused
    for (int i = 0; i < NUM_FDESCS; i++) fdesc_array[i].is_used = 0;
}
```
-   **Detailed Explanation:**
    1.  **Superblock Check:** At startup, `init_fs()` attempts to read block 0 from the SD card, assuming it contains the superblock. It then checks if the `magic` number matches `SUPERBLOCK_MAGIC`.
    2.  **Auto-Formatting:** If the `magic` number does not match (indicating an unformatted disk or a corrupted superblock), `init_fs()` automatically calls `do_mkfs()` to format the file system. After formatting, it re-reads the new superblock.
    3.  **File Descriptor Initialization:** Finally, it marks all internal file descriptor entries (`fdesc_array`) as unused, preparing them for future `open()` calls.

##### Integration in `main.c`

The `init_fs()` function is called early in the kernel's initialization sequence on the primary core (Core 0) in `init/main.c`:

```c
// In init/main.c, inside the if (core_id == 0) block
        // ... other initializations ...

        // Init File System
        init_fs();
        printk("> [INIT] File System initialized.\n");

        // Launch Write-Back Daemon
        char *fs_daemon_argv[] = {"fs_daemon", NULL};
        pid_t wb_pid = do_exec("fs_daemon", 1, fs_daemon_argv, 0);
        printk("> [INIT] FS Write-Back Daemon launched (PID %d).\n", wb_pid);

        // ... rest of initializations ...
```
-   **Role in Kernel Startup:** This ensures that a usable file system is always present when the kernel finishes booting. Following the file system initialization, an `fs_daemon` (filesystem write-back daemon) is launched. This daemon is crucial for implementing the write-back caching policy, periodically calling `sys_fs_sync()` to flush dirty cache blocks to the disk.

### Implementing Basic File and Directory Operations

This section details the implementation of core file and directory manipulation functions, fulfilling the requirements of Task 1 (Physical File System) and Task 2 (File Operations) from the guidebook. These functions leverage the foundational data structures and block-level I/O described in the previous document.

#### 1. Low-Level Block I/O and Bitmap Management

File system operations frequently interact with the underlying storage at the block level and manage resource allocation using bitmaps.

##### `sd_read_block()` and `sd_write_block()`

These functions provide the primary interface for reading and writing 4KB blocks to and from the SD card. Crucially, they integrate with the file system's block cache, transparently handling cache hits, misses, and replacement policies (LRU) to improve performance.

```c
// From kernel/fs/fs.c

// Reads a block from the SD card, utilizing the page cache.
static void sd_read_block(uint32_t block_id, void *buf) { /* ... cache logic ... */ }

// Writes a block to the SD card, utilizing the page cache.
static void sd_write_block(uint32_t block_id, void *buf) { /* ... cache logic ... */ }
```
-   **Filesystem Concept:** These functions abstract away the physical SD card operations, allowing higher-level file system code to operate on logical blocks. The caching mechanism here is a key component for performance, particularly for read/write intensive operations.

##### `alloc_bit()` and `free_bit()`

These functions manage the block and inode bitmaps. They are used to find and allocate free blocks/inodes or mark them as free when no longer needed.

```c
// From kernel/fs/fs.c

// Allocates a free bit (block or inode) from a bitmap.
static int alloc_bit(uint32_t map_offset, uint32_t map_count, uint32_t total_count) { /* ... bitmap logic ... */ }

// Frees a bit (block or inode) in a bitmap.
static void free_bit(uint32_t map_offset, uint32_t index) { /* ... bitmap logic ... */ }
```
-   **Filesystem Concept:** Bitmaps are efficient data structures for tracking the allocation status of fixed-size resources. `alloc_bit` scans the bitmap for the first available bit (represented by `0`) and sets it to `1`, returning its index. `free_bit` does the reverse.

#### 2. Inode Management

Inodes are central to file system operations, storing all metadata about a file or directory.

##### `get_inode()` and `set_inode()`

These functions read an inode structure from the inode table on disk into memory, or write a modified inode structure back to disk.

```c
// From kernel/fs/fs.c

// Reads an inode from the inode table.
static void get_inode(uint32_t ino, inode_t *inode) { /* ... read from disk ... */ }

// Writes an inode to the inode table.
static void set_inode(uint32_t ino, inode_t *inode) { /* ... write to disk ... */ }
```
-   **Filesystem Concept:** Inodes are identified by their unique inode number (`ino`). These functions handle the calculation of the correct block and offset within the inode table to access the specific inode.

##### `get_block_addr()`

This is a critical function for resolving logical block numbers within a file to their physical block addresses on the disk, handling direct and indirect pointers. It can also allocate new blocks if needed.

```c
// From kernel/fs/fs.c

// Translates a logical block number to a physical block address, allocating if 'allocate' is true.
static int get_block_addr(inode_t *inode, uint32_t logical_block, int allocate) { /* ... indexing logic ... */ }
```
-   **Filesystem Concept:** This function implements the multi-level indexing scheme defined in `inode_t`. For small files, it directly accesses `direct_ptrs`. For larger files, it navigates through single or double indirect blocks, reading pointer blocks as necessary. If `allocate` is true, and a required block (either data or pointer block) doesn't exist, it allocates a new one using `alloc_bit` and clears it.

#### 3. Path Resolution and Directory Entry Management

Navigating the directory tree and finding files involves several helper functions.

##### `find_entry()`

Searches for a directory entry (`dentry_t`) by name within a specified directory inode.

```c
// From kernel/fs/fs.c

// Searches for a dentry in a directory, checking dcache first.
static int find_entry(uint32_t dir_ino, char *name, dentry_t *entry) { /* ... dcache and disk lookup ... */ }
```
-   **Filesystem Concept:** Directories store `dentry_t` structures. This function iterates through the data blocks of a directory, looking for a matching name. It first checks the dentry cache (`dcache`) for performance optimization.

##### `lookup_path()`

Resolves a full or relative path to its corresponding inode number.

```c
// From kernel/fs/fs.c

// Resolves a path (e.g., "/a/b/file" or "b/file") to an inode number.
static int lookup_path(char *path) { /* ... path traversal logic ... */ }
```
-   **Filesystem Concept:** This function parses the path components, starting from either the root directory (if absolute path) or the current working directory (`cwd_ino` in the PCB). For each component, it uses `find_entry` to locate the next directory or file's inode.

##### `get_parent_and_name()`

Parses a given path to extract the inode number of its parent directory and the base name of the file/directory.

```c
// From kernel/fs/fs.c

// Splits a path into its parent directory's inode number and the child's name.
static int get_parent_and_name(char *path, uint32_t *parent_ino, char *name) { /* ... parsing logic ... */ }
```
-   **Filesystem Concept:** This helper is crucial for operations like `mkdir`, `rmdir`, `open` (when creating a new file), and `ln`, which require knowing the parent directory to modify its contents.

#### 4. Directory Operations (Task 1)

##### `do_cd()`

Changes the current working directory for the calling process.

```c
// From kernel/fs/fs.c

int do_cd(char *path)
{
    int ino = lookup_path(path); // Resolve path to inode number
    if (ino < 0) return -1; // Path not found
    inode_t inode;
    get_inode(ino, &inode);
    if (inode.mode != IM_DIR) return -1; // Must be a directory
    CURRENT_RUNNING->cwd_ino = ino; // Update PCB's current working directory
    return 0;
}
```
-   **Filesystem Concept:** Each process (`pcb_t`) maintains a `cwd_ino` field, which stores the inode number of its current working directory. `do_cd` updates this field after validating that the target path resolves to an existing directory.

##### `do_ls()`

Lists the contents of a directory. It supports a basic listing and a detailed (`-l`) option.

```c
// From kernel/fs/fs.c

int do_ls(char *path, int option)
{
    // Determine target directory inode: current directory if path is empty, otherwise resolve path
    int ino = (path == NULL || path[0] == '\0') ? CURRENT_RUNNING->cwd_ino : lookup_path(path);
    if (ino < 0) return -1;

    inode_t inode;
    get_inode(ino, &inode);
    if (inode.mode != IM_DIR) return -1; // Only directories can be listed

    char buf[BLOCK_SIZE];
    uint32_t num_dentries = inode.size / sizeof(dentry_t);
    uint32_t per_block = BLOCK_SIZE / sizeof(dentry_t);

    for (uint32_t lb = 0; num_dentries > 0; lb++) {
        int pb = get_block_addr(&inode, lb, 0); // Get physical block for logical block
        if (pb == 0) continue;
        sd_read_block(superblock.data_offset + pb, buf); // Read directory data block
        dentry_t *de = (dentry_t *)buf;
        for (int j = 0; j < per_block && num_dentries > 0; j++, num_dentries--) {
            inode_t target;
            get_inode(de[j].ino, &target); // Get inode for each dentry
            if (option) { // Detailed listing (ls -l)
                printk("[%d] %s  links:%d  size:%d  ino:%d  ", 
                       target.mode, (target.mode == IM_DIR ? "DIR" : "FILE"), 
                       target.nlinks, target.size, de[j].ino);
                // ANSI coloring and icons based on file type
                if (target.mode == IM_DIR) bios_putstr(ANSI_FG_BLUE);
                if (target.mode == IM_DIR) bios_putstr(" "); else bios_putstr(" ");
                bios_putstr(de[j].name);
                if (target.mode == IM_DIR) bios_putstr("/");
                bios_putstr(ANSI_NONE);
                bios_putstr("  ");
                printk("\n");
            } else { // Basic listing
                if (target.mode == IM_DIR) bios_putstr(ANSI_FG_BLUE);
                if (target.mode == IM_DIR) bios_putstr(" "); else bios_putstr(" ");
                bios_putstr(de[j].name);
                if (target.mode == IM_DIR) bios_putstr("/");
                bios_putstr(ANSI_NONE);
                bios_putstr("  ");
            }
        }
    }
    if (!option) bios_putstr("\n\r"); // Newline for basic listing
    pcb_t *curr = CURRENT_RUNNING;
    screen_move_cursor(curr->cursor_x, curr->cursor_y + 1);
    return 0;
}
```
-   **Detailed Explanation:** `do_ls` retrieves the inode of the target directory. It then iterates through the directory's data blocks, reading `dentry_t` entries. For each entry, it retrieves the corresponding inode to get file type, size, and link count. The `option` parameter allows for a simple listing or a more detailed `ls -l` output, including inode mode, links, size, and inode number. Custom ANSI escape codes are used to add color and icons to directory and file names.

##### `do_mkdir()`

Creates a new directory at the specified path.

```c
// From kernel/fs/fs.c

int do_mkdir(char *path)
{
    uint32_t parent_ino;
    char name[MAX_FILE_NAME];
    // Get parent inode and new directory name
    if (get_parent_and_name(path, &parent_ino, name) != 0) return -1;

    dentry_t tmp;
    // Check if entry already exists in parent
    if (find_entry(parent_ino, name, &tmp) == 0) return -1; 

    // Allocate new inode and data block
    int ino = alloc_bit(superblock.inode_map_offset, superblock.inode_map_count, superblock.inode_count);
    int blk = alloc_bit(superblock.block_map_offset, superblock.block_map_count, superblock.data_count);
    if (ino < 0 || blk < 0) return -1;

    // Initialize the new directory's inode
    inode_t new_inode;
    memset(&new_inode, 0, sizeof(inode_t));
    new_inode.ino = ino; new_inode.mode = IM_DIR; new_inode.nlinks = 2; // For . and ..
    new_inode.size = 2 * sizeof(dentry_t);
    new_inode.direct_ptrs[0] = blk; // Assign first data block
    set_inode(ino, &new_inode);

    // Create . and .. entries in the new directory's data block
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    dentry_t *de = (dentry_t *)buf;
    strcpy(de[0].name, "."); de[0].ino = ino;
    strcpy(de[1].name, ".."); de[1].ino = parent_ino;
    sd_write_block(superblock.data_offset + blk, buf);

    // Add new directory's dentry to parent directory
    inode_t parent;
    get_inode(parent_ino, &parent);
    uint32_t dentries_per_block = BLOCK_SIZE / sizeof(dentry_t);
    uint32_t entry_idx = parent.size / sizeof(dentry_t);
    uint32_t blk_idx = entry_idx / dentries_per_block;
    uint32_t off_idx = entry_idx % dentries_per_block;

    // Get a block for the parent directory's dentry, allocating if necessary
    int pb = get_block_addr(&parent, blk_idx, 1); 
    if (pb < 0) return -1;

    sd_read_block(superblock.data_offset + pb, buf);
    de = (dentry_t *)buf;
    strcpy(de[off_idx].name, name);
    de[off_idx].ino = ino;
    sd_write_block(superblock.data_offset + pb, buf);
    
    // Update parent directory's size and link count
    parent.size += sizeof(dentry_t);
    parent.nlinks++; // Parent now has a subdirectory pointing to it via ".."
    set_inode(parent_ino, &parent);

    return 0;
}
```
-   **Detailed Explanation:** `do_mkdir` first ensures the target directory does not already exist. It then allocates a new inode and a data block for the new directory. The new inode is initialized with type `IM_DIR`, a link count of 2 (for `.` and `..`), and its first data block pointer. The new directory's data block is populated with `.` (self-reference) and `..` (parent reference) entries. Finally, a new `dentry_t` for the new directory is added to the parent directory's data blocks, and the parent's inode is updated to reflect the change in size and link count.

##### `do_rmdir()`

Removes an empty directory.

```c
// From kernel/fs/fs.c

int do_rmdir(char *path)
{
    int ino = lookup_path(path);
    if (ino < 0) return -1;
    inode_t inode;
    get_inode(ino, &inode);
    if (inode.mode != IM_DIR) return -1; // Must be a directory
    if (inode.size > 2 * sizeof(dentry_t)) return -1; /* Not empty (has more than just . and ..) */
    return do_rm(path); // Delegates to do_rm for removal logic
}
```
-   **Detailed Explanation:** `do_rmdir` validates that the target is an empty directory (i.e., its size is only for `.` and `..`). It then reuses the general removal logic provided by `do_rm`.

#### 5. File Operations (Task 2)

##### `do_open()`

Opens an existing file or creates a new one.

```c
// From kernel/fs/fs.c

int do_open(char *path, int mode)
{
    int ino = lookup_path(path); // Try to find the file
    if (ino < 0) { // File does not exist
        if (mode == O_RDONLY) return -1; // Cannot create if read-only
        
        // If not found and writable, create the file
        uint32_t parent_ino;
        char name[MAX_FILE_NAME];
        if (get_parent_and_name(path, &parent_ino, name) != 0) return -1; // Get parent and name

        // Allocate new inode
        int new_ino = alloc_bit(superblock.inode_map_offset, superblock.inode_map_count, superblock.inode_count);
        if (new_ino < 0) return -1;
        
        // Initialize new file's inode
        inode_t new_inode;
        memset(&new_inode, 0, sizeof(inode_t));
        new_inode.ino = new_ino; new_inode.mode = IM_REG; new_inode.nlinks = 1;
        new_inode.size = 0;
        set_inode(new_ino, &new_inode); // Write new inode to disk
        
        // Add new file's dentry to parent directory
        inode_t parent;
        get_inode(parent_ino, &parent);
        uint32_t dentries_per_block = BLOCK_SIZE / sizeof(dentry_t);
        uint32_t entry_idx = parent.size / sizeof(dentry_t);
        uint32_t blk_idx = entry_idx / dentries_per_block;
        uint32_t off_idx = entry_idx % dentries_per_block;

        int pb = get_block_addr(&parent, blk_idx, 1); // Get/allocate block in parent for dentry
        char buf[BLOCK_SIZE];
        sd_read_block(superblock.data_offset + pb, buf);
        dentry_t *de = (dentry_t *)buf;
        strcpy(de[off_idx].name, name);
        de[off_idx].ino = new_ino;
        sd_write_block(superblock.data_offset + pb, buf);
        
        parent.size += sizeof(dentry_t); // Update parent size
        set_inode(parent_ino, &parent); // Write updated parent inode
        ino = new_ino; // Use the newly created inode number
    }

    // Allocate a file descriptor
    int fd = -1;
    for (int i = 0; i < NUM_FDESCS; i++) {
        if (!fdesc_array[i].is_used) { fd = i; break; }
    }
    if (fd == -1) return -1; // No free file descriptors

    // Initialize file descriptor
    fdesc_array[fd].is_used = 1;
    fdesc_array[fd].ino = ino;
    fdesc_array[fd].access = mode;
    fdesc_array[fd].read_ptr = 0;
    fdesc_array[fd].write_ptr = 0;
    return fd;
}
```
-   **Detailed Explanation:** `do_open` first attempts to find the file using `lookup_path`. If the file doesn't exist and the `mode` permits writing (e.g., `O_RDWR` or `O_WRONLY`), it proceeds to create the file:
    1.  Allocates a new inode and initializes it as a regular file (`IM_REG`).
    2.  Adds a `dentry_t` for the new file to its parent directory, updating the parent's inode size.
    3.  Once an inode is obtained (either existing or newly created), `do_open` allocates an unused `fdesc_t` (file descriptor) from `fdesc_array`. This descriptor is initialized with the file's inode number, access mode, and `read_ptr`/`write_ptr` set to 0. The index of this `fdesc_t` is returned to the caller.

##### `do_read()`

Reads data from a file identified by a file descriptor.

```c
// From kernel/fs/fs.c

int do_read(int fd, char *buff, int length)
{
    if (fd < 0 || fd >= NUM_FDESCS || !fdesc_array[fd].is_used) return -1; // Invalid FD
    fdesc_t *f = &fdesc_array[fd];
    inode_t inode;
    get_inode(f->ino, &inode); // Get file's inode
    
    // Adjust read length if past end of file or exceeds remaining bytes
    if (f->read_ptr >= inode.size) return 0;
    if (f->read_ptr + length > inode.size) length = inode.size - f->read_ptr;
    
    int read = 0; char buf[BLOCK_SIZE];
    while (read < length) {
        uint32_t lb = f->read_ptr / BLOCK_SIZE; // Logical block number
        uint32_t offset = f->read_ptr % BLOCK_SIZE; // Offset within the block
        uint32_t copy_len = BLOCK_SIZE - offset; // Max bytes to copy from current block
        if (copy_len > length - read) copy_len = length - read; // Limit to remaining read length
        
        int pb = get_block_addr(&inode, lb, 0); // Get physical block address (do not allocate)
        if (pb != 0) { // If block exists
            sd_read_block(superblock.data_offset + pb, buf); // Read block data
            memcpy((uint8_t *)(buff + read), (uint8_t *)(buf + offset), copy_len); // Copy to user buffer
        } else { // If block doesn't exist (hole in file)
            memset(buff + read, 0, copy_len); // Fill with zeros
        }
        f->read_ptr += copy_len; // Advance read pointer
        read += copy_len; // Update total bytes read
    }
    return read;
}
```
-   **Detailed Explanation:** `do_read` validates the file descriptor and retrieves the file's inode. It adjusts the `length` to read, ensuring it doesn't read past the end of the file. It then iteratively reads data block by block:
    1.  Calculates the logical block number (`lb`) and offset within that block from the current `read_ptr`.
    2.  Determines `copy_len`, the amount of data to read from the current block.
    3.  Calls `get_block_addr` to get the physical block address.
    4.  If the block exists, `sd_read_block` is used to read it into a temporary buffer, and data is copied to the user's `buff`. If the block doesn't exist (e.g., a "hole" in a sparse file), the corresponding portion of the user buffer is zeroed out.
    5.  The `read_ptr` is advanced, and the process continues until the requested `length` is read.

##### `do_write()`

Writes data to a file identified by a file descriptor.

```c
// From kernel/fs/fs.c

int do_write(int fd, char *buff, int length)
{
    if (fd < 0 || fd >= NUM_FDESCS || !fdesc_array[fd].is_used) return -1; // Invalid FD
    fdesc_t *f = &fdesc_array[fd];
    inode_t inode;
    get_inode(f->ino, &inode); // Get file's inode
    
    int written = 0; char buf[BLOCK_SIZE];
    while (written < length) {
        uint32_t lb = f->write_ptr / BLOCK_SIZE; // Logical block number
        uint32_t offset = f->write_ptr % BLOCK_SIZE; // Offset within the block
        uint32_t copy_len = BLOCK_SIZE - offset; // Max bytes to copy into current block
        if (copy_len > length - written) copy_len = length - written; // Limit to remaining write length
        
        int pb = get_block_addr(&inode, lb, 1); // Get physical block address, allocating if needed
        if (pb < 0) break; // Error allocating block
        
        sd_read_block(superblock.data_offset + pb, buf); // Read existing block content
        memcpy((uint8_t *)(buf + offset), (uint8_t *)(buff + written), copy_len); // Copy user data into block
        sd_write_block(superblock.data_offset + pb, buf); // Write modified block back
        
        f->write_ptr += copy_len; // Advance write pointer
        written += copy_len; // Update total bytes written
    }
    // Update file size if write extends beyond current size
    if (f->write_ptr > inode.size) { inode.size = f->write_ptr; set_inode(f->ino, &inode); }
    return written;
}
```
-   **Detailed Explanation:** `do_write` operates similarly to `do_read` but for writing. It iterates block by block, calculating `lb`, `offset`, and `copy_len`. The key difference is that `get_block_addr` is called with `allocate = 1`, meaning new data blocks will be allocated if the write operation extends the file or fills a hole. The existing block content is read (`sd_read_block`), the new data is merged into it, and the modified block is written back (`sd_write_block`). After the loop, if the `write_ptr` has surpassed the `inode.size`, the inode's size is updated and written back to disk.

##### `do_close()`

Closes an open file descriptor, releasing its resources.

```c
// From kernel/fs/fs.c

int do_close(int fd)
{
    if (fd < 0 || fd >= NUM_FDESCS) return -1; // Invalid FD
    fdesc_array[fd].is_used = 0; // Mark file descriptor as unused
    return 0;
}
```
-   **Detailed Explanation:** `do_close` simply marks the `fdesc_t` entry corresponding to `fd` as unused, making it available for subsequent `do_open()` calls.

##### `do_lseek()`

Repositions the read/write offset for an open file. (A-core Requirement)

```c
// From kernel/fs/fs.c

int do_lseek(int fd, int offset, int whence)
{
    if (fd < 0 || fd >= NUM_FDESCS || !fdesc_array[fd].is_used) return -1; // Invalid FD
    fdesc_t *f = &fdesc_array[fd];
    inode_t inode;
    get_inode(f->ino, &inode); // Get file's inode
    
    int new_ptr = f->read_ptr; // Start with current read_ptr
    if (whence == SEEK_SET) {
        new_ptr = offset; // Absolute offset from beginning
    } else if (whence == SEEK_CUR) {
        new_ptr += offset; // Relative offset from current position
    } else if (whence == SEEK_END) {
        new_ptr = inode.size + offset; // Relative offset from end of file
    }
    if (new_ptr < 0) new_ptr = 0; // Prevent negative offsets
    f->read_ptr = new_ptr; // Update both read and write pointers
    f->write_ptr = new_ptr;
    return new_ptr; // Return the new offset
}
```
-   **Detailed Explanation:** `do_lseek` allows arbitrary repositioning of the file's read/write pointer. It supports three modes specified by `whence`: `SEEK_SET` (from beginning), `SEEK_CUR` (from current position), and `SEEK_END` (from end of file). Both `read_ptr` and `write_ptr` in the `fdesc_t` are updated to the new position.

##### `do_ln()`

Creates a hard link to an existing file. (A-core Requirement)

```c
// From kernel/fs/fs.c

int do_ln(char *src_path, char *dst_path)
{
    int src_ino = lookup_path(src_path); // Resolve source path
    if (src_ino < 0) return -1; // Source not found
    inode_t src_inode;
    get_inode(src_ino, &src_inode);
    if (src_inode.mode == IM_DIR) return -1; // Cannot link to a directory

    uint32_t parent_ino;
    char name[MAX_FILE_NAME];
    if (get_parent_and_name(dst_path, &parent_ino, name) != 0) return -1; // Get parent and name for destination

    // Add dentry for new link to parent directory
    inode_t parent;
    get_inode(parent_ino, &parent);
    uint32_t entry_idx = parent.size / sizeof(dentry_t);
    int pb = get_block_addr(&parent, entry_idx / (BLOCK_SIZE / sizeof(dentry_t)), 1);
    if (pb < 0) return -1;

    char buf[BLOCK_SIZE];
    sd_read_block(superblock.data_offset + pb, buf);
    dentry_t *de = (dentry_t *)buf;
    strcpy(de[entry_idx % (BLOCK_SIZE / sizeof(dentry_t))].name, name);
    de[entry_idx % (BLOCK_SIZE / sizeof(dentry_t))].ino = src_ino; // Point to source inode
    sd_write_block(superblock.data_offset + pb, buf);

    parent.size += sizeof(dentry_t); // Update parent size
    set_inode(parent_ino, &parent);

    src_inode.nlinks++; // Increment link count of source inode
    set_inode(src_ino, &src_inode);
    return 0;
}
```
-   **Detailed Explanation:** `do_ln` first verifies that the source path exists and is not a directory. It then creates a new `dentry_t` in the destination parent directory. This new dentry points to the *same inode number* as the source file. Finally, the `nlinks` (hard link count) of the target inode is incremented to reflect that another directory entry now refers to it.

##### `do_rm()`

Removes a file or an empty directory. (A-core Requirement)

```c
// From kernel/fs/fs.c

int do_rm(char *path)
{
    uint32_t parent_ino;
    char name[MAX_FILE_NAME];
    if (get_parent_and_name(path, &parent_ino, name) != 0) return -1; // Get parent and name

    inode_t parent;
    get_inode(parent_ino, &parent);
    uint32_t num_dentries = parent.size / sizeof(dentry_t);
    uint32_t per_block = BLOCK_SIZE / sizeof(dentry_t);
    
    int target_ino = -1;
    int found = 0;
    uint32_t target_lb, target_idx;

    // Find the dentry to remove in the parent directory
    for (uint32_t lb = 0; lb * per_block < num_dentries; lb++) {
        int pb = get_block_addr(&parent, lb, 0);
        char buf[BLOCK_SIZE];
        sd_read_block(superblock.data_offset + pb, buf);
        dentry_t *de = (dentry_t *)buf;
        for (uint32_t j = 0; j < per_block && (lb * per_block + j) < num_dentries; j++) {
            if (strcmp(de[j].name, name) == 0) {
                if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return -1; // Cannot remove . or ..
                target_ino = de[j].ino;
                target_lb = lb; target_idx = j;
                found = 1; break;
            }
        }
        if (found) break;
    }
    if (!found) return -1; // Dentry not found

    /* Swap with last dentry in directory to fill the hole */
    uint32_t last_idx = (num_dentries - 1) % per_block;
    uint32_t last_lb = (num_dentries - 1) / per_block;
    if (!(target_lb == last_lb && target_idx == last_idx)) {
        char buf_last[BLOCK_SIZE], buf_target[BLOCK_SIZE];
        int pb_last = get_block_addr(&parent, last_lb, 0);
        int pb_target = get_block_addr(&parent, target_lb, 0);
        sd_read_block(superblock.data_offset + pb_last, buf_last);
        dentry_t *de_last = (dentry_t *)buf_last;
        if (pb_last == pb_target) { // If same block
            dentry_t *de_target = (dentry_t *)buf_last;
            de_target[target_idx] = de_last[last_idx];
            sd_write_block(superblock.data_offset + pb_last, buf_last);
        } else { // If different blocks
            sd_read_block(superblock.data_offset + pb_target, buf_target);
            dentry_t *de_target = (dentry_t *)buf_target;
            de_target[target_idx] = de_last[last_idx];
            sd_write_block(superblock.data_offset + pb_target, buf_target);
        }
    }
    parent.size -= sizeof(dentry_t); // Decrease parent directory size
    if (target_ino != -1) {
        inode_t target;
        get_inode(target_ino, &target);
        if (target.mode == IM_DIR) parent.nlinks--; // Decrement parent's nlinks if directory removed
        set_inode(parent_ino, &parent); // Update parent inode
        target.nlinks--; // Decrement target inode's link count
        if (target.nlinks == 0) { // If no more links, free resources
            free_inode_blocks(&target); // Free data blocks
            free_bit(superblock.inode_map_offset, target_ino); // Free inode bitmap entry
        } else {
            set_inode(target_ino, &target); // Update target inode
        }
    }
    return 0;
}
```
-   **Detailed Explanation:** `do_rm` locates the `dentry_t` for the specified path within its parent directory. It then "removes" the dentry by swapping it with the last valid dentry in the directory's data blocks and decrementing the parent directory's size. If the removed entry was a directory, the parent's `nlinks` is also decremented. Finally, the `nlinks` of the target inode (the file/directory being removed) is decremented. If this count reaches zero, it means no other directory entries point to this inode, so its data blocks are freed (`free_inode_blocks`), and its entry in the inode bitmap is marked as free (`free_bit`).

### Implementing File System Caching

This section covers the implementation of the file system caching module, which addresses **Task 3 (C-core)** requirements from the guidebook. Caching is crucial for improving file system performance by reducing costly access to the slow underlying storage (SD card). The implementation includes both a data (page) cache and a metadata (dentry) cache, along with configurable write policies.

#### 1. Page Cache (Block Cache)

The page cache stores recently accessed data blocks in faster kernel memory. It employs a Least Recently Used (LRU) eviction policy.

##### Data Structure: `page_cache_entry_t`

Each entry in the `page_cache` array represents a cached data block.

```c
// From kernel/fs/fs.c

#define CACHE_SIZE 128 // Number of blocks in the cache

typedef struct page_cache_entry {
    uint32_t block_id;      // The disk block ID stored in this cache entry
    uint8_t valid;          // Is this cache entry valid?
    uint8_t dirty;          // Has this block been modified and needs writing back?
    uint32_t last_access;   // Timestamp of last access (for LRU)
    uint8_t data[BLOCK_SIZE]; // The actual block data (4KB)
} page_cache_entry_t;

static page_cache_entry_t page_cache[CACHE_SIZE];
static uint32_t current_access_time = 0; // Global counter for LRU
static spin_lock_t cache_lock = {UNLOCKED}; // Lock to protect cache access
```
-   **Explanation:** The `page_cache` is an array of `CACHE_SIZE` (128) entries. Each entry holds a full `BLOCK_SIZE` (4KB) of data, along with metadata like the `block_id` it represents, `valid` status, `dirty` flag (indicating modification), and `last_access` timestamp for LRU tracking. Access to the cache is protected by `cache_lock`.

##### Core Cache Logic: `sd_read_block()` and `sd_write_block()`

These functions, previously introduced as low-level block I/O, are now explained in the context of cache interaction.

```c
// From kernel/fs/fs.c

static void flush_block_unlocked(int index)
{
    if (page_cache[index].valid && page_cache[index].dirty) {
        klog("Performing block flushing...\n");
        bios_sd_write(kva2pa((uintptr_t)page_cache[index].data), 
                      SECTOR_PER_BLOCK, 
                      FS_START_SEC + page_cache[index].block_id * SECTOR_PER_BLOCK);
        page_cache[index].dirty = 0; // Clear dirty flag after write-back
    }
}

static int get_cache_index(uint32_t block_id) { /* ... finds entry by block_id ... */ }
static int get_victim_index(void) { /* ... finds invalid or LRU entry ... */ }

static void sd_read_block(uint32_t block_id, void *buf)
{
    spin_lock_acquire(&cache_lock);
    int index = get_cache_index(block_id);
    if (index != -1) { // Cache Hit
        page_cache[index].last_access = ++current_access_time;
        memcpy(buf, page_cache[index].data, BLOCK_SIZE);
        spin_lock_release(&cache_lock);
        return;
    }

    // Cache Miss: Find a victim entry for replacement
    int victim = get_victim_index();
    flush_block_unlocked(victim); // Flush victim if dirty (Write-Back policy implication)

    // Load new block into cache
    page_cache[victim].block_id = block_id;
    page_cache[victim].valid = 1;
    page_cache[victim].dirty = 0; // Newly read block is not dirty
    page_cache[victim].last_access = ++current_access_time;
    bios_sd_read(kva2pa((uintptr_t)page_cache[victim].data), 
                 SECTOR_PER_BLOCK, 
                 FS_START_SEC + block_id * SECTOR_PER_BLOCK);
    memcpy(buf, page_cache[victim].data, BLOCK_SIZE);
    spin_lock_release(&cache_lock);
}

static void sd_write_block(uint32_t block_id, void *buf)
{
    spin_lock_acquire(&cache_lock);
    int index = get_cache_index(block_id);
    if (index == -1) { // Cache Miss
        int victim = get_victim_index();
        flush_block_unlocked(victim); // Flush victim if dirty
        index = victim; // Use victim for new entry
        
        page_cache[index].block_id = block_id;
        page_cache[index].valid = 1;
        // No need to read from disk since we are overwriting the whole block
    }

    // Update Cache
    memcpy(page_cache[index].data, buf, BLOCK_SIZE);
    page_cache[index].last_access = ++current_access_time;

    // Apply write policy
    if (page_cache_policy == CACHE_POLICY_WRITE_THROUGH) {
        page_cache[index].dirty = 0; // Write-Through: always clean after write
        bios_sd_write(kva2pa((uintptr_t)page_cache[index].data), 
                      SECTOR_PER_BLOCK, 
                      FS_START_SEC + block_id * SECTOR_PER_BLOCK);
    } else { // CACHE_POLICY_WRITE_BACK
        page_cache[index].dirty = 1; // Write-Back: mark dirty, write later
    }
    spin_lock_release(&cache_lock);
}
```
-   **Explanation:**
    *   **`flush_block_unlocked(int index)`:** Writes a dirty cache block back to the SD card. This is called when a dirty victim block is chosen for replacement or during a synchronous flush operation.
    *   **`get_cache_index(uint32_t block_id)`:** Checks if a block is already in the cache (cache hit).
    *   **`get_victim_index(void)`:** Selects a cache entry for eviction. It prefers invalid entries first, then uses an LRU (Least Recently Used) algorithm based on `last_access`.
    *   **`sd_read_block()`:** On a cache hit, it updates `last_access` and copies data from cache. On a miss, it selects a victim, flushes it if dirty, reads the new block from SD card into the cache, and then copies data to the caller's buffer.
    *   **`sd_write_block()`:** On a cache hit, it updates the cached data. On a miss, it selects a victim, flushes it if dirty, and then writes the new data into the cache entry. Importantly, it then applies the configured write policy.

#### 2. Dentry Cache (Metadata Cache)

The dentry cache stores mappings from `(parent_ino, name)` to `ino` to speed up path resolution, avoiding repeated disk reads for directory entries.

##### Data Structure: `dcache_entry_t`

```c
// From kernel/fs/fs.c

#define DCACHE_SIZE 128 // Number of entries in the dentry cache

typedef struct dcache_entry {
    uint32_t parent_ino;    // Inode of the parent directory
    uint32_t ino;           // Inode of the child file/directory
    char name[MAX_FILE_NAME]; // Name of the child
    uint8_t valid;          // Is this dcache entry valid?
} dcache_entry_t;

static dcache_entry_t dcache[DCACHE_SIZE];
int dcache_enable = 1; // Flag to enable/disable dcache
```
-   **Explanation:** `dcache` is an array of `DCACHE_SIZE` entries. Each entry stores a mapping for a specific `dentry_t`.

##### Core Dcache Logic

```c
// From kernel/fs/fs.c

static uint32_t dcache_hash(uint32_t parent_ino, char *name)
{
    uint32_t hash = parent_ino;
    while (*name) hash = (hash << 5) + *name++; // Simple hashing algorithm
    return hash % DCACHE_SIZE;
}

static void dcache_add(uint32_t parent_ino, char *name, uint32_t ino)
{
    uint32_t idx = dcache_hash(parent_ino, name);
    dcache[idx].parent_ino = parent_ino;
    dcache[idx].ino = ino;
    strcpy(dcache[idx].name, name);
    dcache[idx].valid = 1;
}

static int dcache_lookup(uint32_t parent_ino, char *name)
{
    uint32_t idx = dcache_hash(parent_ino, name);
    if (dcache[idx].valid && dcache[idx].parent_ino == parent_ino && strcmp(dcache[idx].name, name) == 0) {
        return dcache[idx].ino;
    }
    return -1; // Not found
}

static void dcache_del(uint32_t parent_ino, char *name)
{
    uint32_t idx = dcache_hash(parent_ino, name);
    if (dcache[idx].valid && dcache[idx].parent_ino == parent_ino && strcmp(dcache[idx].name, name) == 0) {
        dcache[idx].valid = 0; // Invalidate entry
    }
}

// In find_entry(uint32_t dir_ino, char *name, dentry_t *entry)
// 1. Look in Dcache (Only if enabled)
    if (dcache_enable) {
        int cached_ino = dcache_lookup(dir_ino, name);
        if (cached_ino != -1) { /* ... cache hit ... */ }
    }
// ... disk lookup ...
// 3. Add to Dcache (Only if enabled)
    if (dcache_enable) {
        dcache_add(dir_ino, name, dentries[j].ino);
    }
```
-   **Explanation:**
    *   **`dcache_hash()`:** A simple hash function to map a `(parent_ino, name)` pair to an index in the `dcache` array.
    *   **`dcache_add()`:** Adds a new dentry mapping to the cache. It uses direct mapping to the hash index; collisions are handled by overwriting, assuming sufficient `DCACHE_SIZE` or less critical data for older entries.
    *   **`dcache_lookup()`:** Checks if a dentry mapping is present in the cache.
    *   **`dcache_del()`:** Invalidates a specific dentry cache entry.
    *   The `find_entry` function (used by `lookup_path`) first attempts a `dcache_lookup`. On a miss, it reads from disk and, if found, adds the new entry to the `dcache`.

#### 3. Cache Policies: Write-Back and Write-Through

The file system supports two write policies, configured dynamically, for the page cache:

*   **`CACHE_POLICY_WRITE_BACK` (1):** Writes to the cache are marked `dirty` and not immediately written to the SD card. They are written back periodically or when a dirty block is evicted. This offers higher write performance but poses a data loss risk on crashes.
*   **`CACHE_POLICY_WRITE_THROUGH` (0):** Writes to the cache are immediately written to the SD card. This ensures higher data reliability but with lower write performance.

The `sd_write_block` function implements this logic, as shown in its snippet above, by checking `page_cache_policy`.

#### 4. Dynamic Configuration via `/proc/sys/vm` and `do_fs_sync()`

The cache policies and parameters can be dynamically adjusted at runtime by modifying a special virtual file `/proc/sys/vm`.

##### `do_mkfs()` initializes `/proc/sys/vm`

When the file system is first formatted (`do_mkfs`), it creates the `/proc/sys/vm` file and populates it with default values:

```c
// From kernel/fs/fs.c, within do_mkfs()

    // ...
    do_mkdir("/proc");
    do_mkdir("/proc/sys");
    int fd = do_open("/proc/sys/vm", O_RDWR);
    if (fd >= 0) {
        char *content = "page_cache_policy = 1\nwrite_back_freq = 30\ndcache_enable = 1\n"; // Added dcache_enable
        do_write(fd, content, strlen(content));
        do_close(fd);
    }
    // ...
```
-   **Explanation:** This snippet creates the directory structure and the `vm` file, setting default `page_cache_policy` to `1` (Write-Back) and `write_back_freq` to `30` seconds, and `dcache_enable` to `1`.

##### `do_fs_sync()`: The Configuration and Flush Mechanism

The `do_fs_sync()` syscall is responsible for reading and applying configuration changes from `/proc/sys/vm`, and for performing periodic flushes for the write-back policy.

```c
// From kernel/fs/fs.c

static char *k_strstr(const char *haystack, const char *needle) { /* ... string search helper ... */ }

void do_fs_sync(void)
{
    // 1. Update Config: Read /proc/sys/vm
    int fd = do_open("/proc/sys/vm", O_RDONLY);
    if (fd >= 0) {
        char buf[256];
        int len = do_read(fd, buf, 255);
        if (len > 0) {
            buf[len] = '\0';
            
            int old_policy = page_cache_policy; // Store old policy for comparison

            // --- Parse page_cache_policy ---
            char *p_policy = k_strstr(buf, "page_cache_policy = ");
            if (p_policy) {
                int new_policy = 0;
                char *p = p_policy + 20; 
                while (*p >= '0' && *p <= '9') { new_policy = new_policy * 10 + (*p - '0'); p++; }
                // Crucial: If switching from Write-Back to Write-Through, flush immediately
                if (old_policy == CACHE_POLICY_WRITE_BACK && new_policy == CACHE_POLICY_WRITE_THROUGH) {
                    spin_lock_acquire(&cache_lock);
                    for (int i = 0; i < CACHE_SIZE; i++) flush_block_unlocked(i);
                    spin_lock_release(&cache_lock);
                }
                page_cache_policy = new_policy;
            }

            // --- Parse write_back_freq ---
            char *p_freq = k_strstr(buf, "write_back_freq = ");
            if (p_freq) { /* ... parse frequency ... */ }

            // --- Parse dcache_enable ---
            char *p_dcache = k_strstr(buf, "dcache_enable = ");
            if (p_dcache) { /* ... parse dcache enable flag ... */ }

            // --- Parse clear_cache ---
            if (k_strstr(buf, "clear_cache = 1")) {
                spin_lock_acquire(&cache_lock);
                for (int i = 0; i < CACHE_SIZE; i++) {
                    flush_block_unlocked(i); // Flush dirty blocks before clearing
                    page_cache[i].valid = 0; // Invalidate entry
                }
                spin_lock_release(&cache_lock);
                for (int i = 0; i < DCACHE_SIZE; i++) { dcache[i].valid = 0; } // Clear Dcache
            }
        }
        do_close(fd);
    }

    // 2. Periodic Flush (for Write-Back policy)
    if (page_cache_policy == CACHE_POLICY_WRITE_BACK) {
        spin_lock_acquire(&cache_lock);
        for (int i = 0; i < CACHE_SIZE; i++) {
            flush_block_unlocked(i); // Flush all dirty blocks
        }
        spin_lock_release(&cache_lock);
    }
}
```
-   **Explanation:**
    *   `do_fs_sync()` opens `/proc/sys/vm`, reads its content, and parses various parameters like `page_cache_policy`, `write_back_freq`, `dcache_enable`, and `clear_cache`.
    *   **Policy Change Handling:** A critical part is detecting a switch from Write-Back to Write-Through. If this occurs, all currently dirty blocks in the page cache are immediately flushed to disk to maintain data consistency.
    *   **Cache Clearing:** If `clear_cache = 1` is detected, both page cache (after flushing dirty blocks) and dentry cache entries are invalidated. This is important for testing "cold read" scenarios.
    *   **Periodic Flush:** Independently of configuration changes, if the `page_cache_policy` is Write-Back, `do_fs_sync()` performs a full flush of all dirty blocks in the cache.

#### 5. Filesystem Daemon (`fs_daemon`)

To ensure the periodic flushing for the write-back policy, a dedicated `fs_daemon` task is launched at kernel startup.

```c
// From test/test_project6/fs_daemon.c

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

void sys_fs_sync(void); // Declared in tiny_libc/include/unistd.h

int main(void)
{
    printf("FS Daemon Started. Sync freq: 30s.\n");
    while (1) {
        sys_sleep(30); // Sleep for the configured write_back_freq (default 30s)
        sys_fs_sync(); // Call the syscall to perform synchronization
    }
    return 0;
}
```
-   **Integration in `main.c`:** The `fs_daemon` is launched on core 0 shortly after file system initialization:
    ```c
    // In init/main.c, inside the if (core_id == 0) block
            // ...
            // Init File System
            init_fs();
            printk("> [INIT] File System initialized.\n");

            // Launch Write-Back Daemon
            char *fs_daemon_argv[] = {"fs_daemon", NULL};
            pid_t wb_pid = do_exec("fs_daemon", 1, fs_daemon_argv, 0);
            printk("> [INIT] FS Write-Back Daemon launched (PID %d).\n", wb_pid);
            // ...
    ```
-   **Explanation:** This daemon runs continuously, sleeping for the `write_back_freq` interval (default 30 seconds) and then invoking `sys_fs_sync()`. This ensures that dirty blocks are periodically written back to the SD card when the write-back policy is active, balancing performance and data integrity.

#### 6. Test Cases (`test/test_project6/cache_test.c`)

The guidebook mandates specific evaluations for the caching module. The provided `cache_test.c` includes functions to demonstrate these effects:

*   **`test_read_cache()`:** Compares "cold read" (cache cleared) vs. "warm read" (data already in cache) performance, illustrating the benefit of the read cache. It uses `clear_cache = 1` in `/proc/sys/vm` to reset the cache.
*   **`test_write_performance()`:** Measures the performance difference between write-through and write-back policies.
*   **`test_metadata()`:** Compares performance of operations (e.g., `sys_open`) with `dcache_enable` on and off, demonstrating the metadata cache's impact.

These tests serve as a verification of the caching implementation's effectiveness.

