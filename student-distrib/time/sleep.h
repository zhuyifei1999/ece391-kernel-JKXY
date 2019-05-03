#ifndef _SLEEP_H
#define _SLEEP_H

struct sleep_spec;

struct sleep_spec *sleep_add(const struct timespec *time);

bool sleep_hashit(const struct sleep_spec *spec);

void sleep_finalize(struct sleep_spec *spec);

#endif
