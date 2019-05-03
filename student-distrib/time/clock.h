#ifndef _CLOCK_H
#define _CLOCK_H

uint64_t mktime64(const unsigned int year0, const unsigned int mon0,
                  const unsigned int day, const unsigned int hour,
                  const unsigned int min, const unsigned int sec);

uint64_t time_now();

#endif
