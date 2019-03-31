#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "task/task.h"
#include "initcall.h"

// ausyscall i386 --dump | awk '{ print "#define NR_LINUX_"$2" "$1 }'
#define NR_LINUX_restart_syscall 0
#define NR_LINUX_exit 1
#define NR_LINUX_fork 2
#define NR_LINUX_read 3
#define NR_LINUX_write 4
#define NR_LINUX_open 5
#define NR_LINUX_close 6
#define NR_LINUX_waitpid 7
#define NR_LINUX_creat 8
#define NR_LINUX_link 9
#define NR_LINUX_unlink 10
#define NR_LINUX_execve 11
#define NR_LINUX_chdir 12
#define NR_LINUX_time 13
#define NR_LINUX_mknod 14
#define NR_LINUX_chmod 15
#define NR_LINUX_lchown 16
#define NR_LINUX_break 17
#define NR_LINUX_oldstat 18
#define NR_LINUX_lseek 19
#define NR_LINUX_getpid 20
#define NR_LINUX_mount 21
#define NR_LINUX_umount 22
#define NR_LINUX_setuid 23
#define NR_LINUX_getuid 24
#define NR_LINUX_stime 25
#define NR_LINUX_ptrace 26
#define NR_LINUX_alarm 27
#define NR_LINUX_oldfstat 28
#define NR_LINUX_pause 29
#define NR_LINUX_utime 30
#define NR_LINUX_stty 31
#define NR_LINUX_gtty 32
#define NR_LINUX_access 33
#define NR_LINUX_nice 34
#define NR_LINUX_ftime 35
#define NR_LINUX_sync 36
#define NR_LINUX_kill 37
#define NR_LINUX_rename 38
#define NR_LINUX_mkdir 39
#define NR_LINUX_rmdir 40
#define NR_LINUX_dup 41
#define NR_LINUX_pipe 42
#define NR_LINUX_times 43
#define NR_LINUX_prof 44
#define NR_LINUX_brk 45
#define NR_LINUX_setgid 46
#define NR_LINUX_getgid 47
#define NR_LINUX_signal 48
#define NR_LINUX_geteuid 49
#define NR_LINUX_getegid 50
#define NR_LINUX_acct 51
#define NR_LINUX_umount2 52
#define NR_LINUX_lock 53
#define NR_LINUX_ioctl 54
#define NR_LINUX_fcntl 55
#define NR_LINUX_mpx 56
#define NR_LINUX_setpgid 57
#define NR_LINUX_ulimit 58
#define NR_LINUX_oldolduname 59
#define NR_LINUX_umask 60
#define NR_LINUX_chroot 61
#define NR_LINUX_ustat 62
#define NR_LINUX_dup2 63
#define NR_LINUX_getppid 64
#define NR_LINUX_getpgrp 65
#define NR_LINUX_setsid 66
#define NR_LINUX_sigaction 67
#define NR_LINUX_sgetmask 68
#define NR_LINUX_ssetmask 69
#define NR_LINUX_setreuid 70
#define NR_LINUX_setregid 71
#define NR_LINUX_sigsuspend 72
#define NR_LINUX_sigpending 73
#define NR_LINUX_sethostname 74
#define NR_LINUX_setrlimit 75
#define NR_LINUX_getrlimit 76
#define NR_LINUX_getrusage 77
#define NR_LINUX_gettimeofday 78
#define NR_LINUX_settimeofday 79
#define NR_LINUX_getgroups 80
#define NR_LINUX_setgroups 81
#define NR_LINUX_select 82
#define NR_LINUX_symlink 83
#define NR_LINUX_oldlstat 84
#define NR_LINUX_readlink 85
#define NR_LINUX_uselib 86
#define NR_LINUX_swapon 87
#define NR_LINUX_reboot 88
#define NR_LINUX_readdir 89
#define NR_LINUX_mmap 90
#define NR_LINUX_munmap 91
#define NR_LINUX_truncate 92
#define NR_LINUX_ftruncate 93
#define NR_LINUX_fchmod 94
#define NR_LINUX_fchown 95
#define NR_LINUX_getpriority 96
#define NR_LINUX_setpriority 97
#define NR_LINUX_profil 98
#define NR_LINUX_statfs 99
#define NR_LINUX_fstatfs 100
#define NR_LINUX_ioperm 101
#define NR_LINUX_socketcall 102
#define NR_LINUX_syslog 103
#define NR_LINUX_setitimer 104
#define NR_LINUX_getitimer 105
#define NR_LINUX_stat 106
#define NR_LINUX_lstat 107
#define NR_LINUX_fstat 108
#define NR_LINUX_olduname 109
#define NR_LINUX_iopl 110
#define NR_LINUX_vhangup 111
#define NR_LINUX_idle 112
#define NR_LINUX_vm86old 113
#define NR_LINUX_wait4 114
#define NR_LINUX_swapoff 115
#define NR_LINUX_sysinfo 116
#define NR_LINUX_ipc 117
#define NR_LINUX_fsync 118
#define NR_LINUX_sigreturn 119
#define NR_LINUX_clone 120
#define NR_LINUX_setdomainname 121
#define NR_LINUX_uname 122
#define NR_LINUX_modify_ldt 123
#define NR_LINUX_adjtimex 124
#define NR_LINUX_mprotect 125
#define NR_LINUX_sigprocmask 126
#define NR_LINUX_create_module 127
#define NR_LINUX_init_module 128
#define NR_LINUX_delete_module 129
#define NR_LINUX_get_kernel_syms 130
#define NR_LINUX_quotactl 131
#define NR_LINUX_getpgid 132
#define NR_LINUX_fchdir 133
#define NR_LINUX_bdflush 134
#define NR_LINUX_sysfs 135
#define NR_LINUX_personality 136
#define NR_LINUX_afs_syscall 137
#define NR_LINUX_setfsuid 138
#define NR_LINUX_setfsgid 139
#define NR_LINUX__llseek 140
#define NR_LINUX_getdents 141
#define NR_LINUX__newselect 142
#define NR_LINUX_flock 143
#define NR_LINUX_msync 144
#define NR_LINUX_readv 145
#define NR_LINUX_writev 146
#define NR_LINUX_getsid 147
#define NR_LINUX_fdatasync 148
#define NR_LINUX__sysctl 149
#define NR_LINUX_mlock 150
#define NR_LINUX_munlock 151
#define NR_LINUX_mlockall 152
#define NR_LINUX_munlockall 153
#define NR_LINUX_sched_setparam 154
#define NR_LINUX_sched_getparam 155
#define NR_LINUX_sched_setscheduler 156
#define NR_LINUX_sched_getscheduler 157
#define NR_LINUX_sched_yield 158
#define NR_LINUX_sched_get_priority_max 159
#define NR_LINUX_sched_get_priority_min 160
#define NR_LINUX_sched_rr_get_interval 161
#define NR_LINUX_nanosleep 162
#define NR_LINUX_mremap 163
#define NR_LINUX_setresuid 164
#define NR_LINUX_getresuid 165
#define NR_LINUX_vm86 166
#define NR_LINUX_query_module 167
#define NR_LINUX_poll 168
#define NR_LINUX_nfsservctl 169
#define NR_LINUX_setresgid 170
#define NR_LINUX_getresgid 171
#define NR_LINUX_prctl 172
#define NR_LINUX_rt_sigreturn 173
#define NR_LINUX_rt_sigaction 174
#define NR_LINUX_rt_sigprocmask 175
#define NR_LINUX_rt_sigpending 176
#define NR_LINUX_rt_sigtimedwait 177
#define NR_LINUX_rt_sigqueueinfo 178
#define NR_LINUX_rt_sigsuspend 179
#define NR_LINUX_pread64 180
#define NR_LINUX_pwrite64 181
#define NR_LINUX_chown 182
#define NR_LINUX_getcwd 183
#define NR_LINUX_capget 184
#define NR_LINUX_capset 185
#define NR_LINUX_sigaltstack 186
#define NR_LINUX_sendfile 187
#define NR_LINUX_getpmsg 188
#define NR_LINUX_putpmsg 189
#define NR_LINUX_vfork 190
#define NR_LINUX_ugetrlimit 191
#define NR_LINUX_mmap2 192
#define NR_LINUX_truncate64 193
#define NR_LINUX_ftruncate64 194
#define NR_LINUX_stat64 195
#define NR_LINUX_lstat64 196
#define NR_LINUX_fstat64 197
#define NR_LINUX_lchown32 198
#define NR_LINUX_getuid32 199
#define NR_LINUX_getgid32 200
#define NR_LINUX_geteuid32 201
#define NR_LINUX_getegid32 202
#define NR_LINUX_setreuid32 203
#define NR_LINUX_setregid32 204
#define NR_LINUX_getgroups32 205
#define NR_LINUX_setgroups32 206
#define NR_LINUX_fchown32 207
#define NR_LINUX_setresuid32 208
#define NR_LINUX_getresuid32 209
#define NR_LINUX_setresgid32 210
#define NR_LINUX_getresgid32 211
#define NR_LINUX_chown32 212
#define NR_LINUX_setuid32 213
#define NR_LINUX_setgid32 214
#define NR_LINUX_setfsuid32 215
#define NR_LINUX_setfsgid32 216
#define NR_LINUX_pivot_root 217
#define NR_LINUX_mincore 218
#define NR_LINUX_madvise 219
#define NR_LINUX_getdents64 220
#define NR_LINUX_fcntl64 221
#define NR_LINUX_gettid 224
#define NR_LINUX_readahead 225
#define NR_LINUX_setxattr 226
#define NR_LINUX_lsetxattr 227
#define NR_LINUX_fsetxattr 228
#define NR_LINUX_getxattr 229
#define NR_LINUX_lgetxattr 230
#define NR_LINUX_fgetxattr 231
#define NR_LINUX_listxattr 232
#define NR_LINUX_llistxattr 233
#define NR_LINUX_flistxattr 234
#define NR_LINUX_removexattr 235
#define NR_LINUX_lremovexattr 236
#define NR_LINUX_fremovexattr 237
#define NR_LINUX_tkill 238
#define NR_LINUX_sendfile64 239
#define NR_LINUX_futex 240
#define NR_LINUX_sched_setaffinity 241
#define NR_LINUX_sched_getaffinity 242
#define NR_LINUX_set_thread_area 243
#define NR_LINUX_get_thread_area 244
#define NR_LINUX_io_setup 245
#define NR_LINUX_io_destroy 246
#define NR_LINUX_io_getevents 247
#define NR_LINUX_io_submit 248
#define NR_LINUX_io_cancel 249
#define NR_LINUX_fadvise64 250
#define NR_LINUX_exit_group 252
#define NR_LINUX_lookup_dcookie 253
#define NR_LINUX_epoll_create 254
#define NR_LINUX_epoll_ctl 255
#define NR_LINUX_epoll_wait 256
#define NR_LINUX_remap_file_pages 257
#define NR_LINUX_set_tid_address 258
#define NR_LINUX_timer_create 259
#define NR_LINUX_timer_settime 260
#define NR_LINUX_timer_gettime 261
#define NR_LINUX_timer_getoverrun 262
#define NR_LINUX_timer_delete 263
#define NR_LINUX_clock_settime 264
#define NR_LINUX_clock_gettime 265
#define NR_LINUX_clock_getres 266
#define NR_LINUX_clock_nanosleep 267
#define NR_LINUX_statfs64 268
#define NR_LINUX_fstatfs64 269
#define NR_LINUX_tgkill 270
#define NR_LINUX_utimes 271
#define NR_LINUX_fadvise64_64 272
#define NR_LINUX_vserver 273
#define NR_LINUX_mbind 274
#define NR_LINUX_get_mempolicy 275
#define NR_LINUX_set_mempolicy 276
#define NR_LINUX_mq_open 277
#define NR_LINUX_mq_unlink 278
#define NR_LINUX_mq_timedsend 279
#define NR_LINUX_mq_timedreceive 280
#define NR_LINUX_mq_notify 281
#define NR_LINUX_mq_getsetattr 282
#define NR_LINUX_sys_kexec_load 283
#define NR_LINUX_waitid 284
#define NR_LINUX_add_key 286
#define NR_LINUX_request_key 287
#define NR_LINUX_keyctl 288
#define NR_LINUX_ioprio_set 289
#define NR_LINUX_ioprio_get 290
#define NR_LINUX_inotify_init 291
#define NR_LINUX_inotify_add_watch 292
#define NR_LINUX_inotify_rm_watch 293
#define NR_LINUX_migrate_pages 294
#define NR_LINUX_openat 295
#define NR_LINUX_mkdirat 296
#define NR_LINUX_mknodat 297
#define NR_LINUX_fchownat 298
#define NR_LINUX_futimesat 299
#define NR_LINUX_fstatat64 300
#define NR_LINUX_unlinkat 301
#define NR_LINUX_renameat 302
#define NR_LINUX_linkat 303
#define NR_LINUX_symlinkat 304
#define NR_LINUX_readlinkat 305
#define NR_LINUX_fchmodat 306
#define NR_LINUX_faccessat 307
#define NR_LINUX_pselect6 308
#define NR_LINUX_ppoll 309
#define NR_LINUX_unshare 310
#define NR_LINUX_set_robust_list 311
#define NR_LINUX_get_robust_list 312
#define NR_LINUX_splice 313
#define NR_LINUX_sync_file_range 314
#define NR_LINUX_tee 315
#define NR_LINUX_vmsplice 316
#define NR_LINUX_move_pages 317
#define NR_LINUX_getcpu 318
#define NR_LINUX_epoll_pwait 319
#define NR_LINUX_utimensat 320
#define NR_LINUX_signalfd 321
#define NR_LINUX_timerfd 322
#define NR_LINUX_eventfd 323
#define NR_LINUX_fallocate 324
#define NR_LINUX_timerfd_settime 325
#define NR_LINUX_timerfd_gettime 326
#define NR_LINUX_signalfd4 327
#define NR_LINUX_eventfd2 328
#define NR_LINUX_epoll_create1 329
#define NR_LINUX_dup3 330
#define NR_LINUX_pipe2 331
#define NR_LINUX_inotify_init1 332
#define NR_LINUX_preadv 333
#define NR_LINUX_pwritev 334
#define NR_LINUX_rt_tgsigqueueinfo 335
#define NR_LINUX_perf_event_open 336
#define NR_LINUX_recvmmsg 337
#define NR_LINUX_fanotify_init 338
#define NR_LINUX_fanotify_mark 339
#define NR_LINUX_prlimit64 340
#define NR_LINUX_name_to_handle_at 341
#define NR_LINUX_open_by_handle_at 342
#define NR_LINUX_clock_adjtime 343
#define NR_LINUX_syncfs 344
#define NR_LINUX_sendmmsg 345
#define NR_LINUX_setns 346
#define NR_LINUX_process_vm_readv 347
#define NR_LINUX_process_vm_writev 348
#define NR_LINUX_kcmp 349
#define NR_LINUX_finit_module 350
#define NR_LINUX_sched_setattr 351
#define NR_LINUX_sched_getattr 352
#define NR_LINUX_renameat2 353
#define NR_LINUX_seccomp 354
#define NR_LINUX_getrandom 355
#define NR_LINUX_memfd_create 356
#define NR_LINUX_bpf 357
#define NR_LINUX_execveat 358
#define NR_LINUX_socket 359
#define NR_LINUX_socketpair 360
#define NR_LINUX_bind 361
#define NR_LINUX_connect 362
#define NR_LINUX_listen 363
#define NR_LINUX_accept4 364
#define NR_LINUX_getsockopt 365
#define NR_LINUX_setsockopt 366
#define NR_LINUX_getsockname 367
#define NR_LINUX_getpeername 368
#define NR_LINUX_sendto 369
#define NR_LINUX_sendmsg 370
#define NR_LINUX_recvfrom 371
#define NR_LINUX_recvmsg 372
#define NR_LINUX_shutdown 373
#define NR_LINUX_userfaultfd 374
#define NR_LINUX_membarrier 375
#define NR_LINUX_mlock2 376
#define NR_LINUX_copy_file_range 377
#define NR_LINUX_preadv2 378
#define NR_LINUX_pwritev2 379
#define NR_LINUX_pkey_mprotect 380
#define NR_LINUX_pkey_alloc 381
#define NR_LINUX_pkey_free 382
#define NR_LINUX_statx 383

#define NR_ECE391_halt    1
#define NR_ECE391_execute 2
#define NR_ECE391_read    3
#define NR_ECE391_write   4
#define NR_ECE391_open    5
#define NR_ECE391_close   6
#define NR_ECE391_getargs 7
#define NR_ECE391_vidmap  8
#define NR_ECE391_set_handler  9
#define NR_ECE391_sigreturn  10

#define MAX_SYSCALL 400

intr_handler_t *syscall_handlers[NUM_SUBSYSTEMS][MAX_SYSCALL];

#define _EXPAND(val) val

#define _DEFINE_SYSCALL(subsystem, name)                                                                                                     \
static void sys_ ## subsystem ## _ ## name ## _wrapper(struct intr_info *info);                                                              \
static void init_sys_ ## subsystem ## _ ## name() {                                                                                          \
    syscall_handlers[_EXPAND(SUBSYSTEM_ ## subsystem)][_EXPAND(NR_ ## subsystem ## _ ## name)] = sys_ ## subsystem ## _ ## name ## _wrapper; \
}                                                                                                                                            \
DEFINE_INITCALL(init_sys_ ## subsystem ## _ ## name, drivers)

#define DEFINE_SYSCALL_COMPLEX(subsystem, name, info_arg)                        \
_DEFINE_SYSCALL(subsystem, name);                                                \
void sys_ ## subsystem ## _ ## name(struct intr_info *info_arg);                 \
static void sys_ ## subsystem ## _ ## name ## _wrapper(struct intr_info *info) { \
    sys_ ## subsystem ## _ ## name(info);                                        \
}                                                                                \
void sys_ ## subsystem ## _ ## name(struct intr_info *info_arg)

#define DEFINE_SYSCALL1(subsystem, name, arg1)                                   \
_DEFINE_SYSCALL(subsystem, name);                                                \
int32_t sys_ ## subsystem ## _ ## name(arg1);                                    \
static void sys_ ## subsystem ## _ ## name ## _wrapper(struct intr_info *info) { \
    info->eax = sys_ ## subsystem ## _ ## name(info->ebx);                       \
}                                                                                \
int32_t sys_ ## subsystem ## _ ## name(arg1)

#define DEFINE_SYSCALL2(subsystem, name, arg1, arg2)                             \
_DEFINE_SYSCALL(subsystem, name);                                                \
int32_t sys_ ## subsystem ## _ ## name(arg1, arg2);                              \
static void sys_ ## subsystem ## _ ## name ## _wrapper(struct intr_info *info) { \
    info->eax = sys_ ## subsystem ## _ ## name(info->ebx, info->ecx);            \
}                                                                                \
int32_t sys_ ## subsystem ## _ ## name(arg1, arg2)

#define DEFINE_SYSCALL3(subsystem, name, arg1, arg2, arg3)                       \
_DEFINE_SYSCALL(subsystem, name);                                                \
int32_t sys_ ## subsystem ## _ ## name(arg1, arg2, arg3);                        \
static void sys_ ## subsystem ## _ ## name ## _wrapper(struct intr_info *info) { \
    info->eax = sys_ ## subsystem ## _ ## name(info->ebx, info->ecx, info->edx); \
}                                                                                \
int32_t sys_ ## subsystem ## _ ## name(arg1, arg2, arg3)

#endif
