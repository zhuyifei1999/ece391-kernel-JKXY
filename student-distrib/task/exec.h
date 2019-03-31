#ifndef _EXEC_H
#define _EXEC_H

#include "task.h"

int32_t do_execve(char *filename, char *argv[], char *envp[]);
int32_t do_execve_heapify(char *filename, char *argv[], char *envp[]);

#endif
