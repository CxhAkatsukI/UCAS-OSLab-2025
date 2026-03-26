#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define debug 1

#define SECTOR_SIZE 512
#define FIXED_APP_SECTORS 15
#define FIXED_KERNEL_SECTORS 15
#define MAX_NAME_LEN 16
#define TASK_MAXNUM 32
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define TASK_NUM_LOC (BOOT_LOADER_SIG_OFFSET - 4)
#define TASK_INFO_START_SECTOR_LOC (BOOT_LOADER_SIG_OFFSET - 6)
#define BATCH_FILE_START_SECTOR_LOC (BOOT_LOADER_SIG_OFFSET - 8)
#define BATCH_IO_BUFFER_LOC (BOOT_LOADER_SIG_OFFSET - 10)
#define IN_BATCH_MODE_LOC (BOOT_LOADER_SIG_OFFSET - 12)
#define BATCH_TASK_INDEX_LOC (BOOT_LOADER_SIG_OFFSET - 14)
#define BATCH_TOTAL_TASKS_LOC (BOOT_LOADER_SIG_OFFSET - 16)
#define SWAP_START_LOC (BOOT_LOADER_SIG_OFFSET - 18)
#define SD_SWAP_SIZE (5 * 1024 * 1024 / 512) // 128 MB in sectors
#define BATCH_FILE_SIZE_SECTORS 2
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

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

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    char name[MAX_NAME_LEN];
    uint16_t start_sector;
    uint16_t size;
    uint32_t byte_offset;
    uint32_t byte_size;
    uint64_t load_address;
    uint32_t task_size;
    uint32_t p_memsz;
} task_info_t;

static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, short tasknum, 
                           short task_info_start_sector, short batch_file_start_sector, FILE *img);

int main(int argc, char **argv)
{
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}

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

        /* virtual memory relevant variables */
        uint32_t p_filesz = 0;
        uint32_t p_memsz = 0;

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

                // Accumulate size for VM loading
                p_filesz += phdr.p_filesz;
                p_memsz += phdr.p_memsz;
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
            taskinfo[taskidx].load_address = 0;

            // Store sizes for VM loading
            taskinfo[taskidx].task_size = p_filesz;
            taskinfo[taskidx].p_memsz = p_memsz;
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

    // 1. Mark the starting sector of the taskinfo array
    short task_info_start_sector = phyaddr / SECTOR_SIZE + 1;
    write_padding(img, &phyaddr, (phyaddr / SECTOR_SIZE + 1) * SECTOR_SIZE);

    // 2. Write the taskinfo array to the end of the image
    fwrite(taskinfo, sizeof(task_info_t), tasknum, img);
    phyaddr += sizeof(task_info_t) * tasknum;

    // 3. Pad to the next sector boundary
    write_padding(img, &phyaddr, (phyaddr / SECTOR_SIZE + 1) * SECTOR_SIZE);

    // 4. Mark the starting sector for the batch file (after the taskinfo)
    short batch_file_start_sector = phyaddr / SECTOR_SIZE + 1;

    // 5. Reserve space for the batch file
    write_padding(img, &phyaddr, phyaddr + (BATCH_FILE_SIZE_SECTORS * SECTOR_SIZE));

    // 6. Write all metadata back into the boot sector
    write_img_info(nbytes_kernel, tasknum, task_info_start_sector, batch_file_start_sector, img);

    // 7. Mark the starting sector for the swap area
    int swap_start_sector = batch_file_start_sector + BATCH_FILE_SIZE_SECTORS + 1;

    // 8. Write this value to the bootblock header
    fseek(img, SWAP_START_LOC, SEEK_SET);
    fwrite(&swap_start_sector, sizeof(int), 1, img);

    // 9. Pad the image with zeros to reserve swap space on the disk
    fseek(img, phyaddr, SEEK_SET);
    write_padding(img, &phyaddr, (swap_start_sector + SD_SWAP_SIZE) * SECTOR_SIZE);

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_img_info(int nbytes_kernel, short tasknum, 
                           short task_info_start_sector, short batch_file_start_sector, FILE *img)
{
    // Write task_info_start_sector
    fseek(img, TASK_INFO_START_SECTOR_LOC, SEEK_SET);
    fwrite(&task_info_start_sector, sizeof(short), 1, img);
#if (debug == 1)
    printf(ANSI_FMT("DEBUG: Writing task_info_start_sector: %d to offset %lx\n", ANSI_FG_CYAN),
           task_info_start_sector, (long)TASK_INFO_START_SECTOR_LOC);
#endif

    // find position for TASK_NUM_LOC
    fseek(img, TASK_NUM_LOC, SEEK_SET);
    fwrite(&tasknum, sizeof(short), 1, img);
#if (debug == 1)
    printf(ANSI_FMT("DEBUG: Writing tasknum: %d to offset %x\n", ANSI_FG_CYAN),
           tasknum, TASK_NUM_LOC);
#endif

    // calc os_size, and find position for OS_SIZE_LOC
    short os_size = NBYTES2SEC(nbytes_kernel);
    fseek(img, OS_SIZE_LOC, SEEK_SET);
    fwrite(&os_size, sizeof(short), 1, img);
#if (debug == 1)
    printf(ANSI_FMT("DEBUG: Writing os_size: %d to offset %x (nbytes_kernel: %d)\n", ANSI_FG_CYAN),
           os_size, OS_SIZE_LOC, nbytes_kernel);
#endif

    // Write BATCH_FILE_START_SECTOR
    fseek(img, BATCH_FILE_START_SECTOR_LOC, SEEK_SET);
    fwrite(&batch_file_start_sector, sizeof(short), 1, img);
#if (debug == 1)
    printf(ANSI_FMT("DEBUG: Writing batch_file_start_sector: %d to offset %lx\n", ANSI_FG_CYAN),
           batch_file_start_sector, (long)BATCH_FILE_START_SECTOR_LOC);
#endif
    // --- Initialize Batch State Variables to Zero ---
    short zero_short = 0;
    uint32_t zero_word = 0;

    fseek(img, BATCH_TOTAL_TASKS_LOC, SEEK_SET);
    fwrite(&zero_short, sizeof(short), 1, img);
    fseek(img, BATCH_TASK_INDEX_LOC, SEEK_SET);
    fwrite(&zero_short, sizeof(short), 1, img);
    fseek(img, IN_BATCH_MODE_LOC, SEEK_SET);
    fwrite(&zero_short, sizeof(short), 1, img);
    fseek(img, BATCH_IO_BUFFER_LOC, SEEK_SET);
    fwrite(&zero_word, sizeof(uint32_t), 1, img);
}

/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}
