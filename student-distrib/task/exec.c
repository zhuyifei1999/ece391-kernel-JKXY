#include "exec.h"
#include "ece391exec_shim.h"
#include "userstack.h"
#include "signal.h"
#include "fp.h"
#include "../char/tty.h"
#include "../char/random.h"
#include "../lib/string.h"
#include "../mm/paging.h"
#include "../mm/kmalloc.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../vfs/path.h"
#include "../cpuid.h"
#include "../panic.h"
#include "../eflags.h"
#include "../err.h"
#include "../errno.h"

static char elf_magic[4] = {0x7f, 0x45, 0x4c, 0x46};

struct elf_header {
    char magic[4];
    uint8_t bit;
    uint8_t endian;
    uint8_t header_version;
    uint8_t abi;
    char padding[8];
    uint16_t type;
    uint16_t isa;
    uint32_t elf_version;
    uint32_t entry_pos;
    uint32_t segment_pos;
    uint32_t section_pos;
    uint32_t flags;
    uint16_t header_size;
    uint16_t segment_entry_size;
    uint16_t segment_entry_num;
    uint16_t section_entry_size;
    uint16_t section_entry_num;
    uint16_t section_name_idx;
};

struct elf_segment {
    uint32_t type;
    uint32_t file_offset;
    uint32_t virt_addr;
    uint32_t undefined;
    uint32_t file_size;
    uint32_t mem_size;
    uint32_t flags;
    uint32_t alignment;
};

// source: <api/linux/auxvec.h>
#define AT_NULL   0    /* end of vector */
#define AT_IGNORE 1    /* entry should be ignored */
#define AT_EXECFD 2    /* file descriptor of program */
#define AT_PHDR   3    /* program headers for program */
#define AT_PHENT  4    /* size of program header entry */
#define AT_PHNUM  5    /* number of program headers */
#define AT_PAGESZ 6    /* system page size */
#define AT_BASE   7    /* base address of interpreter */
#define AT_FLAGS  8    /* flags */
#define AT_ENTRY  9    /* entry point of program */
#define AT_NOTELF 10    /* program is not ELF */
#define AT_UID    11    /* real uid */
#define AT_EUID   12    /* effective uid */
#define AT_GID    13    /* real gid */
#define AT_EGID   14    /* effective gid */
#define AT_PLATFORM 15  /* string identifying CPU for optimizations */
#define AT_HWCAP  16    /* arch dependent hints at CPU capabilities */
#define AT_CLKTCK 17    /* frequency at which times() increments */
/* AT_* values 18 through 22 are reserved */
#define AT_SECURE 23   /* secure mode boolean */
#define AT_BASE_PLATFORM 24    /* string identifying real platform, may
                 * differ from AT_PLATFORM. */
#define AT_RANDOM 25    /* address of 16 random bytes */
#define AT_HWCAP2 26    /* extension of AT_HWCAP */

#define AT_EXECFN 31    /* filename of program */

// <uapi/asm/auxvec.h>
#define AT_SYSINFO		32
#define AT_SYSINFO_EHDR		33

struct auxv {
    int type;
    long val;
};

static int32_t decode_elf(struct file *exe, struct elf_header *header) {
    int32_t res;

    res = filp_seek(exe, 0, SEEK_SET);
    if (res < 0)
        return res;
    if (res != 0)
        return -ENOEXEC;

    res = filp_read(exe, header, sizeof(*header));
    if (res < 0)
        return res;
    if (res != sizeof(*header))
        return -ENOEXEC;

    if (strncmp(header->magic, elf_magic, sizeof(elf_magic)))
        return -ENOEXEC;
    if (header->bit != 1)
        return -ENOEXEC;
    if (header->endian != 1)
        return -ENOEXEC;
    if (header->header_version != 1)
        return -ENOEXEC;
    if (header->abi != 0)
        return -ENOEXEC;
    if (header->type != 2)
        return -ENOEXEC;
    if (header->isa != 3)
        return -ENOEXEC;
    if (header->elf_version != 1)
        return -ENOEXEC;
    if (header->header_size != sizeof(struct elf_header))
        return -ENOEXEC;
    if (header->segment_entry_size != sizeof(struct elf_segment))
        return -ENOEXEC;

    return 0;
}

static inline __always_inline uint32_t hwcap() {
    uint32_t a, d;
    cpuid(1, &a, &d);
    return d;
}

/*
 *   do_execve
 *   DESCRIPTION: do execution
 *   INPUTS: char *filename, char *argv[], char *envp[]
 *   RETURN VALUE: int32_t error code
 */
int32_t do_execve(char *filename, char *argv[], char *envp[]) {
    struct file *exe = filp_open(filename, 0, 0); // TODO: permission check for execute bit
    // check if exe is valid
    if (IS_ERR(exe))
        return PTR_ERR(exe);

    int32_t ret = 0;
    // check for magic
    char buf[sizeof(elf_magic)];
    ret = filp_read(exe, buf, sizeof(elf_magic));
    if (ret < 0)
        goto err_close;

    // check if the content in buf is correct
    if (strncmp(buf, elf_magic, sizeof(elf_magic))) {
        ret = -ENOEXEC;
        goto err_close;
    }

    struct elf_header header;

    ret = decode_elf(exe, &header);
    if (ret)
        goto err_close;

    enum subsystem subsystem;

    // TDDO: Any better way?
    if (header.segment_entry_num == 3)
        subsystem = SUBSYSTEM_ECE391;
    else
        subsystem = SUBSYSTEM_LINUX;

    // ECE391 subsystem cannot have more than 6 userspace processes
    // Grading criteria. Nothing can be said...
    if (subsystem == SUBSYSTEM_ECE391) {
        uint32_t ece391_cnt = 0;

        struct list_node *node;
        list_for_each(&tasks, node) {
            struct task_struct *task = node->value;
            if (task->mm && task->subsystem == SUBSYSTEM_ECE391 && task != current)
                ece391_cnt++;
        }

        if (ece391_cnt >= 6) {
            ret = -EAGAIN;
            goto err_close;
        }
    }

    filp_seek(exe, 0, SEEK_SET);

    if (!current->cwd)
        current->cwd = filp_open("/", 0, 0);

    if (IS_ERR(current->cwd)) {
        ret = PTR_ERR(current->cwd);
        goto err_close;
    }

    // TODO: start of point of no return. If anything fails here deliver SIGSEGV

    *current->entry_regs = (struct intr_info){
        .eflags = EFLAGS_BASE | IF,
        .cs = USER_CS,
        .ds = USER_DS,
        .es = USER_DS,
        .fs = USER_DS,
        .gs = USER_DS,
        .ss = USER_DS,
    };

    // replace current's executable pointer
    if (current->exe)
        filp_close(current->exe);
    current->exe = exe;

    // check if the current thread have a file descriptor table
    if (current->files) {
        // so it's inherited
        uint32_t i;
        array_for_each(&current->files->files, i) {
            struct file *file = array_get(&current->files->files, i);
            if (file && (file->flags & O_CLOEXEC)) {
                // close the file if necessary
                filp_close(file);
                array_set(&current->files->files, i, NULL);
            }
        }
    } else {
        // if not, give it a default set of file descriptors
        current->files = kmalloc(sizeof(*current->files));
        *current->files = (struct files_struct){
            .refcount = ATOMIC_INITIALIZER(1),
        };
        switch (subsystem) {
        case SUBSYSTEM_LINUX:;
            struct file *tty = filp_open_anondevice(TTY_CURRENT, O_RDWR, S_IFCHR | 0666);
            array_set(&current->files->files, 0, tty);
            array_set(&current->files->files, 1, tty);
            array_set(&current->files->files, 2, tty);
            atomic_add(&tty->refcount, 2);
            break;
        case SUBSYSTEM_ECE391:
            array_set(&current->files->files, 0, filp_open_anondevice(TTY_CURRENT, 0, S_IFCHR | 0666));
            array_set(&current->files->files, 1, filp_open_anondevice(TTY_CURRENT, O_WRONLY, S_IFCHR | 0666));
            break;
        }
    }

    // new page directory
    page_directory_t *new_pagedir = new_directory();
    if (current->mm) {
        if (!atomic_dec(&current->mm->refcount)) {
            // free the page directory
            free_directory(current->mm->page_directory);
            kfree(current->mm);
        }
    }
    // malloc the mm for current
    current->mm = kmalloc(sizeof(*current->mm));
    *current->mm = (struct mm_struct){
        .brk = 0,
        .page_directory = new_pagedir,
        .refcount = ATOMIC_INITIALIZER(1),
    };

    memset(current->ldt, 0, sizeof(current->ldt));
    memset(current->gdt_tls, 0, sizeof(current->gdt_tls));

    if (current->fxsave_data) {
        kfree(current->fxsave_data);
        current->fxsave_data = NULL;
    }
    finit();

    current->subsystem = subsystem;

    set_current_comm(list_peek_back(&exe->path->components));

    switch_directory(new_pagedir);

    switch (subsystem) {
    case SUBSYSTEM_LINUX: {
        uint32_t i;
        uint32_t pos = header.segment_pos;
        int32_t res;
        uint32_t file_hdraddr = 0;

        for (i = 0; i < header.segment_entry_num; i++) {
            struct elf_segment segment;

            res = filp_seek(exe, pos, SEEK_SET);
            if (res != pos)
                goto force_sigsegv;

            res = filp_read(exe, &segment, sizeof(segment));
            if (res != sizeof(segment))
                goto force_sigsegv;

            switch (segment.type) {
            case 1: {
                // LOAD
                if (segment.file_size > segment.mem_size)
                    segment.file_size = segment.mem_size;
                if (!segment.mem_size)
                    continue;

                uint32_t mapaddr = PAGE_IDX_ADDR(PAGE_IDX(segment.virt_addr));
                uint32_t numpages = PAGE_IDX(segment.mem_size + segment.virt_addr - mapaddr - 1) + 1;
                if (!request_pages((void *)mapaddr, numpages, GFP_USER | ((segment.flags & 2) ? 0 : GFP_RO)))
                    goto force_sigsegv;

                res = filp_seek(exe, segment.file_offset, SEEK_SET);
                if (res != segment.file_offset)
                    goto force_sigsegv;

                res = filp_read(exe, (void *)segment.virt_addr, segment.file_size);
                if (res != segment.file_size)
                    goto force_sigsegv;

                if (segment.flags & 2)
                    if (!current->mm->brk || segment.file_size != segment.mem_size)
                        current->mm->brk = segment.virt_addr + segment.mem_size;

                if (!segment.file_offset)
                    file_hdraddr = segment.virt_addr;

                break;
            }
            case 2: // DYNAMIC
            case 3: // INTERP
                goto force_sigsegv; // TODO
            }

            pos += header.segment_entry_size;
        }

        current->entry_regs->eip = header.entry_pos;

        // TODO: Any better way to make a better stack address?
        uint32_t stack_page = (2U << 30) - PAGE_SIZE_LARGE;
        if (!request_pages((void *)stack_page, 1, GFP_USER | GFP_LARGE))
            goto force_sigsegv;
        current->entry_regs->esp = stack_page + PAGE_SIZE_LARGE;

        // Now setup the stack

        // Map VDSO
        // uint32_t vdso_addr = 2U << 30;
        // extern unsigned char vdso_start, vdso_end, vsyscall;
        // if (!request_pages((void *)vdso_addr, 1, GFP_USER | GFP_RO))
        //     goto force_sigsegv;
        // memcpy((void *)vdso_addr, &vdso_start, &vdso_end - &vdso_start);

        uint32_t envp_len = 0;
        if (envp)
            for (; envp[envp_len]; envp_len++);

        char **envp_user = kcalloc(envp_len + 1, sizeof(*envp_user));
        for (i = 0; i < envp_len; i++) {
            push_userstack(current->entry_regs, envp[i], strlen(envp[i]) + 1);
            envp_user[i] = (void *)current->entry_regs->esp;
        }

        uint32_t argv_len = 0;
        if (argv)
            for (; argv[argv_len]; argv_len++);

        char **argv_user = kcalloc(argv_len + 1, sizeof(*argv_user));
        for (i = 0; i < argv_len; i++) {
            push_userstack(current->entry_regs, argv[i], strlen(argv[i]) + 1);
            argv_user[i] = (void *)current->entry_regs->esp;
        }

        for (i = 0; i < 16; i++) {
            int32_t rand = rdrand();
            push_userstack(current->entry_regs, &rand, sizeof(rand));
        }
        uint32_t rand_ptr = current->entry_regs->esp;

        // Now align the stack
        current->entry_regs->esp &= ~0xf;

        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_NULL }, sizeof(struct auxv));
        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_UID, .val = 0 }, sizeof(struct auxv));
        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_EUID, .val = 0 }, sizeof(struct auxv));
        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_GID, .val = 0 }, sizeof(struct auxv));
        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_EGID, .val = 0 }, sizeof(struct auxv));
        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_RANDOM, .val = rand_ptr }, sizeof(struct auxv));
        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_PHDR, .val = file_hdraddr + header.segment_pos }, sizeof(struct auxv));
        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_PHENT, .val = sizeof(struct elf_segment) }, sizeof(struct auxv));
        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_PHNUM, .val = header.segment_entry_num }, sizeof(struct auxv));;
        push_userstack(current->entry_regs, // TODO: is this correct?
            &(struct auxv){ .type = AT_BASE, .val = file_hdraddr }, sizeof(struct auxv));
        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_ENTRY, .val = header.entry_pos }, sizeof(struct auxv));
        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_PAGESZ, .val = PAGE_SIZE_SMALL }, sizeof(struct auxv));
        push_userstack(current->entry_regs,
            &(struct auxv){ .type = AT_HWCAP, .val = hwcap() }, sizeof(struct auxv));
        // push_userstack(current->entry_regs,
        //     &(struct auxv){ .type = AT_SYSINFO, .val = vdso_addr + (&vsyscall - &vdso_start) }, sizeof(struct auxv));
        // push_userstack(current->entry_regs,
        //     &(struct auxv){ .type = AT_SYSINFO_EHDR, .val = vdso_addr }, sizeof(struct auxv));

        push_userstack(current->entry_regs, envp_user, (envp_len + 1) * sizeof(*envp_user));
        push_userstack(current->entry_regs, argv_user, (argv_len + 1) * sizeof(*argv_user));

        kfree(envp_user);
        kfree(argv_user);

        push_userstack(current->entry_regs, &argv_len, sizeof(argv_len));

        // TODO: EDX = a function pointer that the application should register with atexit (BA_OS)
        break;
    }
    case SUBSYSTEM_ECE391:
        // ECE391 subsystem always map the file to 0x08048000,
        // with stack bottom at the end of the page
        request_pages((void *)ECE391_PAGEADDR, 1, GFP_USER | GFP_LARGE);
        filp_seek(exe, 0, SEEK_SET);
        filp_read(exe, (void *)(ECE391_PAGEADDR + ECE391_MAPADDR), LEN_4M - ECE391_MAPADDR);
        current->entry_regs->eip = *(uint32_t *)(ECE391_PAGEADDR + ECE391_MAPADDR + 24);
        current->entry_regs->esp = ECE391_PAGEADDR + LEN_4M;
        // copy the argv to ECE391_PAGEADDR 0x08000000
        if (argv && argv[0] && argv[1])
            strncpy((char *)ECE391_ARGSADDR, argv[1], ECE391_MAPADDR);
        else
            *(char *)ECE391_ARGSADDR = '\0';

        break;
    }

err_close:
    if (ret)
        filp_close(exe);

    return ret;

force_sigsegv:
    force_sig(current, SIGSEGV);
    return 0;
}
