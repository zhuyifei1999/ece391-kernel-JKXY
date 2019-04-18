#include "../syscall.h"
#include "../errno.h"

// We don't really have creds, so make sure root is the only supported value
DEFINE_SYSCALL0(LINUX, geteuid32) {
    return 0;
}
DEFINE_SYSCALL0(LINUX, getuid32) {
    return 0;
}
DEFINE_SYSCALL0(LINUX, getegid32) {
    return 0;
}
DEFINE_SYSCALL0(LINUX, getgid32) {
    return 0;
}

DEFINE_SYSCALL1(LINUX, setuid32, uint32_t, uid) {
    if (uid)
        return -EINVAL;
    return 0;
}
DEFINE_SYSCALL1(LINUX, setgid32, uint32_t, gid) {
    if (gid)
        return -EINVAL;
    return 0;
}
