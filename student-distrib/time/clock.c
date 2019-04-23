#include "../drivers/rtc.h"
#include "../lib/stdint.h"
#include "../mm/paging.h"
#include "../syscall.h"

// source: kernel/time/time.c
uint64_t mktime64(const unsigned int year0, const unsigned int mon0,
                  const unsigned int day, const unsigned int hour,
                  const unsigned int min, const unsigned int sec) {
    unsigned int mon = mon0, year = year0;

    /* 1..12 -> 11,12,1..10 */
    if (0 >= (int) (mon -= 2)) {
        mon += 12;    /* Puts Feb last since it has leap day */
        year -= 1;
    }

    return ((((uint64_t)
          (year/4 - year/100 + year/400 + 367*mon/12 + day) +
          year*365 - 719499
        )*24 + hour /* now have hours - midnight tomorrow handled here */
      )*60 + min /* now have minutes */
    )*60 + sec; /* finally seconds */
}

uint64_t time_now() {
    struct rtc_timestamp timestamp;
    rtc_get_timestamp(&timestamp);
    uint64_t time = mktime64(
        timestamp.year,
        timestamp.month,
        timestamp.day,
        timestamp.hour,
        timestamp.minute,
        timestamp.second
    );

    return time;
}

DEFINE_SYSCALL1(LINUX, time, uint64_t *, tloc) {
    uint64_t time = time_now();

    if (safe_buf(tloc, sizeof(*tloc), true) == sizeof(*tloc))
        *tloc = time;

    return time;
}
