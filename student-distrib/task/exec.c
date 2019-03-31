#include "exec.h"
#include "../lib/string.h"
#include "../mm/paging.h"
#include "../mm/kmalloc.h"
#include "../vfs/file.h"
#include "../eflags.h"
#include "../err.h"
#include "../errno.h"

static char elf_magic[4] = {0x7f, 0x45, 0x4c, 0x46};

int32_t do_execve(char *filename, char *argv[], char *envp[]) {
    struct file *exe = filp_open(filename, 0, 0); // TODO: permission check for execute bit
    if (IS_ERR(exe))
        return PTR_ERR(exe);

    int32_t ret = 0;
    // check for magic
    char buf[sizeof(elf_magic)];
    ret = filp_read(exe, buf, sizeof(elf_magic));
    if (ret < 0)
        goto err_close;
    if (strncmp(buf, elf_magic, sizeof(elf_magic))) {
        ret = -ENOEXEC;
        goto err_close;
    }

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

    if (current->exe)
        filp_close(current->exe);
    current->exe = exe;

    uint32_t i;
    array_for_each(&current->files->files, i) {
        struct file *file = array_get(&current->files->files, i);
        if (file && (file->flags & O_CLOEXEC))
            filp_close(file);
        array_set(&current->files->files, i, NULL);
    }

    page_directory_t *new_pagedir = new_directory();
    if (current->mm) {
        if (!atomic_dec(&current->mm->refcount)) {
            free_directory(current->mm->page_directory);
            kfree(current->mm);
        }
    }
    current->mm = kmalloc(sizeof(current->mm));
    current->mm->page_directory = new_pagedir;
    atomic_set(&current->mm->refcount, 1);

    current->subsystem = subsystem;

    // TODO: current->comm. set to last component of path?

    switch_directory(new_pagedir);

    switch (subsystem) {
    case SUBSYSTEM_LINUX:
        // TODO
        // TODO: EDX = a function pointer that the application should register with atexit (BA_OS)
        break;
    case SUBSYSTEM_ECE391:;
        // ECE391 subsystem always map the file to 0x08048000,
        // with stack bottom at the end of the page
#define ECE391_PAGEADDR 0x08000000
#define ECE391_MAPADDR 0x48000
        request_pages((void *)ECE391_PAGEADDR, 1, GFP_USER | GFP_LARGE);
        filp_read(exe, (void *)(ECE391_PAGEADDR + ECE391_MAPADDR), LEN_4M - ECE391_MAPADDR);
        regs.eip = *(uint32_t *)(ECE391_PAGEADDR + ECE391_MAPADDR + 24);
        regs.esp = ECE391_PAGEADDR + LEN_4M;
    }

    set_all_regs(&regs);

err_close:
    filp_close(exe);
    return ret;
}
