#include "inet.h"
#include "ip.h"
#include "udp.h"
#include "../vfs/file.h"
#include "../vfs/poll.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../mm/kmalloc.h"
#include "../structure/array.h"
#include "../printk.h"
#include "../initcall.h"
#include "../syscall.h"
#include "../err.h"
#include "../errno.h"

// source: <linux/socket.h>
#define AF_UNIX  1  /* Unix domain sockets */
#define AF_INET  2  /* Internet IP Protocol */
#define AF_INET6 10 /* IP version 6 */

// source: <linux/net.h>
enum sock_type {
    SOCK_STREAM    = 1,
    SOCK_DGRAM     = 2,
    SOCK_RAW       = 3,
    SOCK_RDM       = 4,
    SOCK_SEQPACKET = 5,
    SOCK_DCCP      = 6,
    SOCK_PACKET    = 10,
};
#define SOCK_TYPE_MASK 0xf

// source: <uapi/linux/in.h>
enum {
    IPPROTO_IP      = 0,   /* Dummy protocol for TCP */
    IPPROTO_ICMP    = 1,   /* Internet Control Message Protocol */
    IPPROTO_IGMP    = 2,   /* Internet Group Management Protocol */
    IPPROTO_IPIP    = 4,   /* IPIP tunnels (older KA9Q tunnels use 94) */
    IPPROTO_TCP     = 6,   /* Transmission Control Protocol */
    IPPROTO_EGP     = 8,   /* Exterior Gateway Protocol */
    IPPROTO_PUP     = 12,  /* PUP protocol */
    IPPROTO_UDP     = 17,  /* User Datagram Protocol */
    IPPROTO_IDP     = 22,  /* XNS IDP protocol */
    IPPROTO_TP      = 29,  /* SO Transport Protocol Class 4 */
    IPPROTO_DCCP    = 33,  /* Datagram Congestion Control Protocol */
    IPPROTO_IPV6    = 41,  /* IPv6-in-IPv4 tunnelling */
    IPPROTO_RSVP    = 46,  /* RSVP Protocol */
    IPPROTO_GRE     = 47,  /* Cisco GRE tunnels (rfc 1701,1702) */
    IPPROTO_ESP     = 50,  /* Encapsulation Security Payload protocol */
    IPPROTO_AH      = 51,  /* Authentication Header protocol */
    IPPROTO_MTP     = 92,  /* Multicast Transport Protocol */
    IPPROTO_BEETPH  = 94,  /* IP option pseudo header for BEET */
    IPPROTO_ENCAP   = 98,  /* Encapsulation Header */
    IPPROTO_PIM     = 103, /* Protocol Independent Multicast */
    IPPROTO_COMP    = 108, /* Compression Header Protocol */
    IPPROTO_SCTP    = 132, /* Stream Control Transport Protocol */
    IPPROTO_UDPLITE = 136, /* UDP-Lite (RFC 3828) */
    IPPROTO_MPLS    = 137, /* MPLS in IP (RFC 4023) */
    IPPROTO_RAW     = 255, /* Raw IP packets */
    IPPROTO_MAX
};

struct sockaddr_in {
    uint16_t sin_family; /* address family: AF_INET */
    uint16_t sin_port;   /* port in network byte order */
    ip_addr_t sin_addr;   /* internet address */
    // uint8_t  sin_zero[8];
};

// sysctl: net.ipv4.ip_local_port_range
#define DYN_PORT_MIN 32768
#define DYN_PORT_MAX 61000

static uint16_t allocate_port() {
    static uint32_t port_next = DYN_PORT_MIN;
    uint16_t ret = port_next++;

    if (port_next > DYN_PORT_MAX)
        port_next = DYN_PORT_MIN;

    return ret;
}

struct udp_socket {
    ip_addr_t host_addr;
    uint16_t host_port;
    ip_addr_t remote_addr;
    uint16_t remote_port;
    char *recv_buf;
    uint32_t buf_size;
    struct task_struct *task;
};

struct array udp_sockets;

static uint16_t udp_allocate_port() {
    uint16_t port;
    do {
        port = allocate_port();
    } while (array_get(&udp_sockets, port));
    return port;
}

static int32_t udp_read(struct file *file, char *buf, uint32_t nbytes) {
    struct udp_socket *socket = file->vendor;

    if (socket->task) {
        return -EBUSY;
    }
    socket->task = current;

    current->state = TASK_INTERRUPTIBLE;
    while (!socket->recv_buf && !signal_pending(current))
        schedule();
    current->state = TASK_RUNNING;

    socket->task = NULL;

    if (signal_pending(current))
        return -EINTR;

    if (nbytes > socket->buf_size)
        nbytes = socket->buf_size;

    // Don't let network interrupts interrupt us, or the buffer is gonna be messed up.
    // FIXME: This is badly inefficient.
    cli();
    memcpy(buf, socket->recv_buf, nbytes);
    socket->buf_size -= nbytes;
    if (socket->buf_size) {
        char *recv_buf_new = kmalloc(socket->buf_size);
        memcpy(recv_buf_new, socket->recv_buf + nbytes, socket->buf_size);
        kfree(socket->recv_buf);
        socket->recv_buf = recv_buf_new;
    } else {
        kfree(socket->recv_buf);
        socket->recv_buf = NULL;
    }
    sti();

    return nbytes;
}

static int32_t udp_write(struct file *file, const char *buf, uint32_t nbytes) {
    struct udp_socket *socket = file->vendor;

    // TODO: Split packets when necessary
    udp_send_packet(&socket->remote_addr, socket->host_port, socket->remote_port, buf, nbytes);

    return nbytes;
}

static int32_t udp_open(struct file *file, struct inode *inode) {
    struct udp_socket *socket = kcalloc(1, sizeof(*socket));
    if (!socket)
        return -ENOMEM;

    file->vendor = socket;
    return 0;
}

static void udp_release(struct file *file) {
    struct udp_socket *socket = file->vendor;
    if (socket->host_port)
        array_set(&udp_sockets, socket->host_port, NULL);

    if (socket->recv_buf)
        kfree(socket->recv_buf);

    kfree(socket);
}

static void udp_poll_cb(struct poll_entry *poll_entry) {
    struct udp_socket *socket = poll_entry->file->vendor;

    if (socket->task == current) {
        socket->task = NULL;
    }
}
static int32_t udp_poll(struct file *file, struct poll_entry *poll_entry) {
    struct udp_socket *socket = file->vendor;

    if (poll_entry->events & POLLOUT)
        poll_entry->revents |= POLLOUT;

    if (poll_entry->events & POLLIN) {
        if (!socket->task) {
            socket->task = current;
        }

        if (socket->task == current && socket->recv_buf)
            poll_entry->revents |= POLLIN;

        if (!list_contains(&poll_entry->cleanup_cb, &udp_poll_cb))
            list_insert_back(&poll_entry->cleanup_cb, &udp_poll_cb);
    }

    return 0;
}

void udp_receive(ip_addr_t *src_ip, uint16_t src_port, uint16_t dst_port, const void *data, uint32_t nbytes) {
    struct udp_socket *socket = array_get(&udp_sockets, dst_port);
    if (!socket)
        return; // TODO: send back 'connection refused' ICMP packet

    char *recv_buf_new = kmalloc(socket->buf_size + nbytes);
    if (socket->buf_size) {
        memcpy(recv_buf_new, socket->recv_buf, socket->buf_size);
        kfree(socket->recv_buf);
    }

    memcpy(recv_buf_new + socket->buf_size, data, nbytes);
    socket->recv_buf = recv_buf_new;
    socket->buf_size += nbytes;

    if (socket->task)
        wake_up_process(socket->task);
}

static struct file_operations udp_sock_op = {
    .read    = &udp_read,
    .write   = &udp_write,
    .poll    = &udp_poll,
    .open    = &udp_open,
    .release = &udp_release,
};

DEFINE_SYSCALL3(LINUX, socket, int, domain, int, type, int, protocol) {
    if (domain != AF_INET)
        return -EINVAL;

    if ((type & SOCK_TYPE_MASK) != SOCK_DGRAM)
        return -EINVAL;

    int32_t res;

    struct file *file = filp_open_dummy(&udp_sock_op, NULL, NULL, O_RDWR, S_IFSOCK | 0700);
    if (IS_ERR(file)) {
        res = PTR_ERR(file);
        goto out;
    }

    for (res = 0;; res++) {
        if (
            !array_get(&current->files->files, res) &&
            !array_set(&current->files->files, res, file)
        )
            break;
    }

    array_set(&current->files->cloexec, res, (void *)(type & O_CLOEXEC));

out:
    return res;
}

DEFINE_SYSCALL3(LINUX, bind, int, fd, const struct sockaddr_in *, addr, uint32_t, addrlen) {
    struct file *file = array_get(&current->files->files, fd);
    if (!file)
        return -EBADF;
    if (file->op != &udp_sock_op)
        return -ENOTSOCK;

    if (addrlen < sizeof(*addr))
        return -EINVAL;
    if (safe_buf(addr, sizeof(*addr), false) != sizeof(*addr))
        return -EFAULT;
    if (addr->sin_family != AF_INET)
        return -EINVAL;

    struct udp_socket *socket = file->vendor;

    memcpy(&socket->host_addr, &addr->sin_addr, sizeof(addr->sin_addr));
    uint16_t new_port = ntohs(addr->sin_port);
    if (!new_port) {
        if (socket->host_port)
            new_port = socket->host_port;
        else
            new_port = udp_allocate_port();
    }
    if (new_port != socket->host_port) {
        array_set(&udp_sockets, socket->host_port, NULL);
        socket->host_port = new_port;
        array_set(&udp_sockets, socket->host_port, socket);
    }

    return 0;
}

DEFINE_SYSCALL3(LINUX, connect, int, fd, const struct sockaddr_in *, addr, uint32_t, addrlen) {
    struct file *file = array_get(&current->files->files, fd);
    if (!file)
        return -EBADF;
    if (file->op != &udp_sock_op)
        return -ENOTSOCK;

    if (addrlen < sizeof(*addr))
        return -EINVAL;
    if (safe_buf(addr, sizeof(*addr), false) != sizeof(*addr))
        return -EFAULT;
    if (addr->sin_family != AF_INET)
        return -EINVAL;

    struct udp_socket *socket = file->vendor;

    memcpy(&socket->remote_addr, &addr->sin_addr, sizeof(addr->sin_addr));
    socket->remote_port = ntohs(addr->sin_port);

    return 0;
}

#define SYS_SOCKET      1               /* sys_socket(2)                */
#define SYS_BIND        2               /* sys_bind(2)                  */
#define SYS_CONNECT     3               /* sys_connect(2)               */
#define SYS_LISTEN      4               /* sys_listen(2)                */
#define SYS_ACCEPT      5               /* sys_accept(2)                */
#define SYS_GETSOCKNAME 6               /* sys_getsockname(2)           */
#define SYS_GETPEERNAME 7               /* sys_getpeername(2)           */
#define SYS_SOCKETPAIR  8               /* sys_socketpair(2)            */
#define SYS_SEND        9               /* sys_send(2)                  */
#define SYS_RECV        10              /* sys_recv(2)                  */
#define SYS_SENDTO      11              /* sys_sendto(2)                */
#define SYS_RECVFROM    12              /* sys_recvfrom(2)              */
#define SYS_SHUTDOWN    13              /* sys_shutdown(2)              */
#define SYS_SETSOCKOPT  14              /* sys_setsockopt(2)            */
#define SYS_GETSOCKOPT  15              /* sys_getsockopt(2)            */
#define SYS_SENDMSG     16              /* sys_sendmsg(2)               */
#define SYS_RECVMSG     17              /* sys_recvmsg(2)               */
#define SYS_ACCEPT4     18              /* sys_accept4(2)               */

DEFINE_SYSCALL2(LINUX, socketcall, uint32_t, call, void *, args) {
    switch (call) {
    case SYS_SOCKET: {
        struct sys_socket_args {
            int domain;
            int type;
            int protocol;
        };

        if (safe_buf(args, sizeof(struct sys_socket_args), false) != sizeof(struct sys_socket_args))
            return -EFAULT;

        struct sys_socket_args *args_s = args;
        return sys_LINUX_socket(args_s->domain, args_s->type, args_s->protocol);
    }
    case SYS_BIND: {
        struct sys_bind_args {
            int fd;
            const struct sockaddr_in *addr;
            uint32_t addrlen;
        };

        if (safe_buf(args, sizeof(struct sys_bind_args), false) != sizeof(struct sys_bind_args))
            return -EFAULT;

        struct sys_bind_args *args_s = args;
        return sys_LINUX_bind(args_s->fd, args_s->addr, args_s->addrlen);
    } // TODO
    case SYS_CONNECT: {
        struct sys_connect_args {
            int fd;
            const struct sockaddr_in *addr;
            uint32_t addrlen;
        };

        if (safe_buf(args, sizeof(struct sys_connect_args), false) != sizeof(struct sys_connect_args))
            return -EFAULT;

        struct sys_connect_args *args_s = args;
        return sys_LINUX_connect(args_s->fd, args_s->addr, args_s->addrlen);
    }
    default:
        // printk("%s[%d]: Unknown socketcall: %u\n", current->comm, current->pid, call);
        return -ENOSYS;
    }
}

static void init_socket() {
    fill_default_file_op(&udp_sock_op);
}
DEFINE_INITCALL(init_socket, drivers);
