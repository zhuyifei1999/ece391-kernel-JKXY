#include "syscall.h"
#include "lib/string.h"
#include "mm/paging.h"
#include "x86_desc.h"
#include "errno.h"

// Yeah, certain user programs check this
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};
static const struct utsname utsname = {
    .sysname = "Linux",
    .nodename = "localhost",
    .release = "5.0-ece391",
    .version = "#0 EC BUILD",
    .machine = "QEMU",
    .domainname = "localdomain",
};

DEFINE_SYSCALL1(LINUX, uname, struct utsname *, buf) {
    uint32_t nbytes = safe_buf(buf, sizeof(*buf), true);
    if (nbytes != sizeof(*buf))
        return -EFAULT;

    *buf = utsname;

    return 0;
}
