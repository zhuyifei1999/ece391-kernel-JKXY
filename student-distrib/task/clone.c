#include "clone.h"
#include "exit.h"
#include "session.h"
#include "sched.h"
#include "signal.h"
#include "tls.h"
#include "../mm/kmalloc.h"
#include "../lib/string.h"
#include "../eflags.h"
#include "../panic.h"
#include "../x86_desc.h"
#include "../syscall.h"
#include "../initcall.h"
#include "../err.h"
#include "../errno.h"

/*
 *   _next_pid
 *   DESCRIPTION: loop over all pid numbers
 *   RETURN VALUE: next pid
 *   SIDE EFFECTS: change the return value of the next call
 */
static uint16_t _next_pid() {
    static uint16_t pid = 1;
    unsigned long flags;

    cli_and_save(flags);
    uint16_t ret = pid;
    // circularly get the next pid
    pid++;
    if (pid > MAXPID)
        pid = LOOPPID;
    restore_flags(flags);

    return ret;
}

/*
 *   next_pid
 *   DESCRIPTION: get next available pid number
 *   RETURN VALUE: next available pid number
 *   SIDE EFFECTS: change the return value of _next_pid()
 */
static uint16_t next_pid() {
    uint16_t pid;
    do {
        pid = _next_pid();
    } while (PTR_ERR(get_task_from_pid(pid)) != -ESRCH);
    return pid;
}

/*
 *   clone_entry_handler
 *   DESCRIPTION: child entry point
 */
static void clone_entry_handler(struct intr_info *info) {
    uint32_t flags = info->eax;
    int (*fn)(void *args) = (void *)info->ebx;
    void *args = (void *)info->ecx;
    int *tidptr = (void *)info->edx;
    struct user_desc *newtls = (void *)info->esi;

    if (flags & CLONE_PARENT_SETTID && tidptr)
        *tidptr = current->pid;

    if (flags & CLONE_SETTLS && newtls)
        do_set_thread_area(newtls);

    current->entry_regs = info;

    if (fn) {
        int exitcode = (*fn)(args);
        if (exitcode || current->entry_regs->cs == KERNEL_CS)
            do_exit(exitcode);
    } else {
        *info = *(struct intr_info *)args;
        kfree(args);
    }

    if (current->entry_regs->cs == KERNEL_CS)
        BUG();

    // return to userspace
}

static void init_clone_entry() {
    intr_setaction(INTR_ENTRY, (struct intr_action){
        .handler = &clone_entry_handler } );
}
DEFINE_INITCALL(init_clone_entry, early);

/*
 *   do_clone
 *   DESCRIPTION: clone a child task from the parent
 *   INPUTS: uint32_t flags, int (*fn)(void *args), void *args, int *ptid, int *ctid
 *   RETURN VALUE: next available pid number
 *   SIDE EFFECTS: none
 */
struct task_struct *do_clone(uint32_t flags, int (*fn)(void *args), void *args, int *ptid, int *ctid, struct user_desc *newtls) {
    struct task_struct *task = alloc_pages(TASK_STACK_PAGES, TASK_STACK_PAGES_POW, 0);
    if (!task)
        return ERR_PTR(-ENOMEM);
    // TODO: handle OOMs, if fail I think they should just be SIGSEGV-ed

    // increase reference count
    if (current->cwd)
        atomic_inc(&current->cwd->refcount);
    if (current->exe)
        atomic_inc(&current->exe->refcount);
    if (current->session)
        atomic_inc(&current->session->refcount);

    // set new pid, and copy the other task state
    *task = (struct task_struct){
        .pid       = next_pid(),
        .ppid      = (flags & CLONE_PARENT) ? current->ppid : current->pid,
        .state     = TASK_RUNNING,
        .subsystem = current->subsystem,
        .cwd       = current->cwd,
        .exe       = current->exe,
        .session   = current->session,
        .pgid      = current->pgid,
    };

    strncpy(task->comm, current->comm, sizeof(task->comm));

    // if share the memory, increase the reference count of these pages. otherwise get new cow memory
    if (current->mm) {
        if (flags & CLONE_VM) {
            atomic_inc(&current->mm->refcount);
            task->mm = current->mm;
        } else {
            task->mm = kmalloc(sizeof(*task->mm));
            *task->mm = (struct mm_struct){
                .brk = current->mm->brk,
                .page_directory = clone_directory(current->mm->page_directory),
                .refcount = ATOMIC_INITIALIZER(1),
            };
        }

        // although irrelevant, only those with mm can have TLS, right?
        memcpy(task->ldt, current->ldt, sizeof(task->ldt));
        memcpy(task->gdt_tls, current->gdt_tls, sizeof(task->gdt_tls));
    }

    // if share the files, increase the reference count of these files. otherwise copy the file table
    if (current->files) {
        if (flags & CLONE_FILES) {
            atomic_inc(&current->files->refcount);
            task->files = current->files;
        } else {
            task->files = kmalloc(sizeof(*task->files));
            *task->files = (struct files_struct){
                .refcount = ATOMIC_INITIALIZER(1),
            };
            uint32_t i;
            // loop over all files
            array_for_each(&current->files->files, i) {
                struct file *file = array_get(&current->files->files, i);
                if (file) {
                    array_set(&task->files->files, i, file);
                    atomic_inc(&file->refcount);
                }
            }
        }
    }

    if (flags & CLONE_SIGHAND) {
        atomic_inc(&current->sigactions->refcount);
        task->sigactions = current->sigactions;
    } else {
        task->sigactions = kmalloc(sizeof(*task->sigactions));
        memcpy(task->sigactions->sigactions, current->sigactions->sigactions,
            sizeof(current->sigactions->sigactions));
        atomic_set(&task->sigactions->refcount, 1);
    }

    // The child needs to shared the same FP registers (also irrelevant with mm, but...)
    if (current->mm) {
        task->fxsave_data = kmalloc(sizeof(*task->fxsave_data));
        if (current->fxsave_data)
            memcpy(task->fxsave_data, current->fxsave_data, sizeof(*task->fxsave_data));
        else
            fxsave(task->fxsave_data);
    }

    if (flags & CLONE_PARENT_SETTID && ptid)
        *ptid = task->pid;

    // The position of this struct intr_info controls where the stack is going
    // to end up, not any of the contents in the struct, bacause we are using
    // the same code segment.
    struct intr_info *regs = (void *)((uint32_t)task +
        TASK_STACK_PAGES * PAGE_SIZE_SMALL - sizeof(struct intr_info));
    // function prototype don't matter here
    extern void entry_task(void);
    *regs = (struct intr_info){
        .eax    = (uint32_t)flags,
        .ebx    = (uint32_t)fn,
        .ecx    = (uint32_t)args,
        .edx    = (uint32_t)ctid,
        .esi    = (uint32_t)newtls,
        .eflags = EFLAGS_BASE | IF,
        .eip    = (uint32_t)&entry_task,
        .cs     = KERNEL_CS,
        .ds     = KERNEL_DS,
        .es     = KERNEL_DS,
        .fs     = KERNEL_DS,
        .gs     = KERNEL_DS,
    };
    // store registers for scheduler to switch to new task
    task->return_regs = regs;

    // so we know this PID is used
    list_insert_back(&tasks, task);

    return task;
}

/*
 *   kernel_thread
 *   DESCRIPTION: make a new kernel thread
 *   INPUTS: int (*fn)(void *args), void *args
 *   RETURN VALUE: new task
 */
struct task_struct *kernel_thread(int (*fn)(void *args), void *args) {
    return do_clone(SIGCHLD, fn, args, NULL, NULL, NULL);
}

DEFINE_SYSCALL_COMPLEX(LINUX, clone, regs) {
    uint32_t flags = regs->ebx;
    uint32_t child_stack = regs->ecx;
    int *ptid = (void *)regs->edx;
    struct user_desc *newtls = (void *)regs->esi;
    int *ctid = (void *)regs->edi;

    if (ptid && safe_buf(ptid, sizeof(*ptid), true) != sizeof(*ptid))
        ptid = NULL;
    if (ctid && safe_buf(ctid, sizeof(*ctid), true) != sizeof(*ctid))
        ctid = NULL;
    if (newtls && safe_buf(newtls, sizeof(*newtls), false) != sizeof(*newtls))
        newtls = NULL;

    struct intr_info *newregs = kmalloc(sizeof(*newregs));
    if (!newregs) {
        regs->eax = -ENOMEM;
        return;
    }
    *newregs = *regs;

    newregs->eax = 0;

    if (child_stack)
        newregs->esp = (uint32_t)child_stack;

    struct task_struct *task = do_clone(flags, NULL, newregs, ptid, ctid, newtls);
    if (IS_ERR(task)) {
        kfree(newregs);
        regs->eax = PTR_ERR(task);
    } else {
        regs->eax = task->pid;
        wake_up_process(task);
        printk("%d %d\n", current->pid, task->pid);
    }
}
