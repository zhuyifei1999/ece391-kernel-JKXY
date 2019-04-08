#include "exec.h"
#include "ece391exec_shim.h"
#include "../lib/string.h"
#include "../mm/paging.h"
#include "../mm/kmalloc.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../vfs/path.h"
#include "../panic.h"
#include "../eflags.h"
#include "../err.h"
#include "../errno.h"

static char elf_magic[4] = {0x7f, 0x45, 0x4c, 0x46};

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

    ret = 0;

    // TODO: determine subsystem
    enum subsystem subsystem = SUBSYSTEM_ECE391;

    filp_seek(exe, 0, SEEK_SET);
    // TODO: start of point of no return. If anything fails here deliver SIGSEGV

    struct intr_info regs = {
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
                filp_close(file);
                //replace current's execution by function's arg
                array_set(&current->files->files, i, NULL);
            }
        }
    } else {
        // if not, give it a default set of file descriptors
        current->files = kmalloc(sizeof(*current->files));
        atomic_set(&current->files->refcount, 1);
        current->files->files = (struct array){0};
        switch (subsystem) {
        case SUBSYSTEM_LINUX:;
            struct file *tty = filp_open_anondevice(MKDEV(5, 0), 0, S_IFCHR | 0666);
            array_set(&current->files->files, 0, tty);
            array_set(&current->files->files, 1, tty);
            array_set(&current->files->files, 2, tty);
            atomic_add(&tty->refcount, 2);
            break;
        case SUBSYSTEM_ECE391:
            array_set(&current->files->files, 0, filp_open_anondevice(MKDEV(5, 0), 0, S_IFCHR | 0666));
            array_set(&current->files->files, 1, filp_open_anondevice(MKDEV(5, 0), O_WRONLY, S_IFCHR | 0666));
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
    current->mm->page_directory = new_pagedir;
    atomic_set(&current->mm->refcount, 1);
    current->subsystem = subsystem;

    // check if the content and pointer of argv are valid
    if (argv && argv[0] && *argv[0])
        strncpy(current->comm, argv[0], sizeof(current->comm) - 1);
    else
        strncpy(current->comm, list_peek_back(&exe->path->components), sizeof(current->comm) - 1);

    switch_directory(new_pagedir);

    switch (subsystem) {
    case SUBSYSTEM_LINUX:
        // TODO
        // TODO: EDX = a function pointer that the application should register with atexit (BA_OS)
        break;
    case SUBSYSTEM_ECE391:;
        // ECE391 subsystem always map the file to 0x08048000,
        // with stack bottom at the end of the page
        request_pages((void *)ECE391_PAGEADDR, 1, GFP_USER | GFP_LARGE);
        filp_read(exe, (void *)(ECE391_PAGEADDR + ECE391_MAPADDR), LEN_4M - ECE391_MAPADDR);
        regs.eip = *(uint32_t *)(ECE391_PAGEADDR + ECE391_MAPADDR + 24);
        regs.esp = ECE391_PAGEADDR + LEN_4M;
        // copy the argv to ECE391_PAGEADDR 0x08000000
        if (argv && argv[0] && argv[1])
            strncpy((char *)ECE391_ARGSADDR, argv[1], ECE391_MAPADDR);
        else
            *(char *)ECE391_ARGSADDR = '\0';

        break;
    }

err_close:
    kfree(filename);

    // free argv
    uint32_t i;
    if (argv) {
        for (i = 0; argv[i]; i++)
            kfree(argv[i]);
        kfree(argv);
    }

    // free envp
    if (envp) {
        for (i = 0; envp[i]; i++)
            kfree(envp[i]);
        kfree(envp);
    }

    // errored
    if (ret) {
        filp_close(exe);
        return ret;
    }

    set_all_regs(&regs);
}

// TODO: ENOMEM
/*
 *   do_execve_heapify
 *   DESCRIPTION: do the execution and store mempry in heap
 *   instead of stack
 *   INPUTS: char *filename, char *argv[], char *envp[]
 *   RETURN VALUE: int32_t error code
 */
int32_t do_execve_heapify(char *filename, char *argv[], char *envp[]) {
    // do_execve expect all args to be on heap so it can free it. if it's not, use this instead.
    char *filename_h = strdup(filename);

    char **argv_h = NULL;
    char **envp_h = NULL;

    // if argv is valid
    if (argv) {
        uint32_t length;
        for (length = 0; argv[length]; length++);
        argv_h = kcalloc(length + 1, sizeof(*argv));
        // copy the argv to heap malloc space in kernel
        for (length = 0; argv[length]; length++)
            argv_h[length] = strdup(argv[length]);
    }

    // if enviroment pointer is valid
    if (envp) {
        uint32_t length;
        for (length = 0; envp[length]; length++);
        envp_h = kcalloc(length + 1, sizeof(*envp));
        // copy the envp to heap malloc space in kernel
        for (length = 0; envp[length]; length++)
            envp_h[length] = strdup(envp[length]);
    }

    return do_execve(filename_h, argv_h, envp_h);
}
