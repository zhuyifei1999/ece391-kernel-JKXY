#include "random.h"
#include "../lib/string.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../initcall.h"

#if !HAS_RDRAND
// FIXME: This should be seeded
uint16_t rand() { // RAND_MAX assumed to be 32767
    static unsigned long int next = 1;
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}
#endif

static int32_t random_read(struct file *file, char *buf, uint32_t nbytes) {
    int32_t ret = nbytes;
    while (nbytes) {
        uint32_t rand = rdrand();
        if (nbytes < sizeof(rand)) {
            memcpy(buf, &rand, nbytes);
            break;
        }

        *(uint32_t *)buf = rand;
        buf += sizeof(rand);
        nbytes -= sizeof(rand);
    }
    return ret;
}

static struct file_operations random_dev_op = {
    .read = &random_read,
};

static void init_random_char() {
    register_dev(S_IFCHR, MKDEV(1, 8), &random_dev_op); // /dev/random
    register_dev(S_IFCHR, MKDEV(1, 9), &random_dev_op); // /dev/urandom
}
DEFINE_INITCALL(init_random_char, drivers);
