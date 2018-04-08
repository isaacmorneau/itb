#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/epoll.h>

#include "wrapper.h"

//==>fd ioctl wrappers<==
void set_fd_limit() {
    struct rlimit lim;
    //the kernel patch that allows for RLIM_INFINITY to work breaks stuff
    //so we are restricted to finite values,
    //this was found as the exact max via trial and error
    lim.rlim_cur = (1UL << 20);
    lim.rlim_max = (1UL << 20);
    ensure(setrlimit(RLIMIT_NOFILE, &lim) != -1);
}

void set_non_blocking (int sfd) {
    int flags;
    ensure((flags = fcntl(sfd, F_GETFL, 0)) != -1);
    flags |= O_NONBLOCK;
    ensure(fcntl(sfd, F_SETFL, flags) != -1);
}

void set_fast(int sfd) {
    int i = 1;
    setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(i));
    setsockopt(sfd, IPPROTO_TCP, TCP_QUICKACK, &i, sizeof(i));
}

//==>tcp wrappers<==
void set_listening(int sfd) {
    ensure(listen(sfd, SOMAXCONN) != -1);
}
int make_connected(const char * address, const char * port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd = -1;

    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     // Return IPv4 and IPv6 choices
    hints.ai_socktype = SOCK_STREAM; // We want a TCP socket
    hints.ai_flags = AI_PASSIVE;     // All interfaces

    ensure(getaddrinfo(address, port, &hints, &result) == 0);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if ((sfd = socket(rp->ai_family, rp->ai_socktype|SOCK_CLOEXEC, rp->ai_protocol)) == -1) {
            continue;
        }

        if((s = connect(sfd, rp->ai_addr, rp->ai_addrlen)) == 0) {
            break;
        }

        close(sfd);
    }

    ensure(rp != NULL);

    freeaddrinfo(result);

    set_non_blocking(sfd);
    return sfd;
}

int make_bound_tcp(const char * port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd = -1;

    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     // Return IPv4 and IPv6 choices
    hints.ai_socktype = SOCK_STREAM; // We want a TCP socket
    hints.ai_flags = AI_PASSIVE;     // All interfaces

    //NULL host will bind to local
    ensure(getaddrinfo(NULL, port, &hints, &result) == 0);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if ((sfd = socket(rp->ai_family, rp->ai_socktype|SOCK_CLOEXEC, rp->ai_protocol)) == -1) {
            continue;
        }

        int enable = 1;
        ensure(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != -1);

        if (!bind(sfd, rp->ai_addr, rp->ai_addrlen)) {
            //we managed to bind successfully
            break;
        }

        close(sfd);
    }

    ensure(rp != NULL);

    freeaddrinfo(result);

    set_non_blocking(sfd);
    return sfd;
}

//==>ip wrappers<==
void make_storage(struct sockaddr_storage * restrict addr, const char * restrict host, int port) {
    struct addrinfo hints;
    struct addrinfo * rp;

    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;// All interfaces

    //null the service as it only accepts strings and we have the port already
    ensure(getaddrinfo(host, NULL, &hints, &rp) == 0);

    //assuming the first result returned will be correct
    //TODO find a way to check
    ensure(rp);

    //add the port manually
    if (rp->ai_family == AF_INET) {
        ((struct sockaddr_in *)rp->ai_addr)->sin_port = htons(port);
    } else if (rp->ai_family == AF_INET6) {
        ((struct sockaddr_in6 *)rp->ai_addr)->sin6_port = htons(port);
    }

    memcpy(addr, rp->ai_addr, rp->ai_addrlen);

    freeaddrinfo(rp);
}

//==>udp wrappers<==
int make_bound_udp(int port) {
    struct sockaddr_in sin;
    int sockfd;

    ensure((sockfd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0)) != -1);

    memset(&sin, 0, sizeof(sin));
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    sin.sin_family = AF_INET;

    int enable = 1;
    ensure(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != -1);
    ensure(bind(sockfd, (struct sockaddr *) &sin, sizeof(sin)) != -1);

    return sockfd;
}

int read_message(int sockfd, char * restrict buffer, int len) {
    int total = 0, ret;
readmsg:
    ensure_nonblock((ret = recvfrom(sockfd, buffer + total, len - total, 0, NULL, NULL)) != -1);
    if (ret == -1)
        return total;
    total += ret;
    goto readmsg;
}

int read_message_port(int sockfd, char * restrict buffer, int len, int * port) {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int total = 0, ret;
readmsg:
    addr_len = sizeof(struct sockaddr_storage);
    ensure_nonblock((ret = recvfrom(sockfd, buffer + total, len - total, 0, (struct sockaddr *)&addr,
                    &addr_len)) != -1);
    if (ret == -1)
        return total;
    total += ret;
    ensure(getnameinfo((struct sockaddr *)&addr, addr_len, hbuf, sizeof(hbuf), sbuf, sizeof(hbuf), NI_NUMERICHOST|NI_NUMERICSERV|NI_DGRAM) == 0);
    *port = atoi(sbuf);
    goto readmsg;
}

int read_message_addr(int sockfd, char * restrict buffer, int len, struct sockaddr_storage * addr) {
    socklen_t addr_len;
    int total = 0, ret;
readmsg:
    addr_len = sizeof(struct sockaddr_storage);
    ensure_nonblock((ret = recvfrom(sockfd, buffer + total, len - total, 0, (struct sockaddr *)addr,
                    &addr_len)) != -1);
    if (ret == -1)
        return total;
    total += ret;
    goto readmsg;
}

int send_message(int sockfd, const char * restrict buffer, int len, const struct sockaddr_storage * restrict addr) {
    int ret;
    socklen_t addr_len = sizeof(struct sockaddr_storage);
    ensure_nonblock((ret = sendto(sockfd, buffer, len, 0, (struct sockaddr *)addr, addr_len)) != -1);
    return ret;
}

//==>epoll wrappers<==
int make_epoll() {
    int efd;
    ensure((efd = epoll_create1(EPOLL_CLOEXEC)) != -1);
    return efd;
}

int wait_epoll(int efd, struct epoll_event * restrict events) {
    int ret;
    ensure((ret = epoll_wait(efd, events, MAXEVENTS, -1)) != -1);
    return ret;
}

int wait_epoll_timeout(int efd, struct epoll_event * restrict events, int timeout) {
    int ret;
    ensure((ret = epoll_wait(efd, events, MAXEVENTS, timeout)) != -1);
    return ret;
}

int add_epoll_ptr(int efd, int ifd, void * ptr) {
    int ret;
    static struct epoll_event event;
    event.data.ptr = ptr;
    event.events = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

int add_epoll_ptr_flags(int efd, int ifd, void * ptr, int flags) {
    int ret;
    static struct epoll_event event;
    event.data.ptr = ptr;
    event.events = flags;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

int add_epoll_fd(int efd, int ifd) {
    int ret;
    static struct epoll_event event;
    event.data.fd = ifd;
    event.events = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

int add_epoll_fd_flags(int efd, int ifd, int flags) {
    int ret;
    static struct epoll_event event;
    event.data.fd = ifd;
    event.events = flags;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

//==>connection forwarding wrappers<==
int forward_read(const directional_buffer * con) {
    int total = 0, ret;
read:
    //read new data
    ensure_nonblock((ret = splice(con->sockfd, 0, con->pipefd[1], 0, UINT_MAX, SPLICE_F_MORE | SPLICE_F_NONBLOCK)) != -1);
    if (ret == -1) {//eagain all data read
        return total;
    } else if (ret == 0) {//actually means the connection was closed
        return 0;//notify caller that the connection closed
    }

    //forward the data out
readflush:
    ensure_nonblock((ret = splice(con->pipefd[0], 0, con->paired->sockfd, 0, UINT_MAX, SPLICE_F_MORE | SPLICE_F_NONBLOCK)) != -1);
    if (ret <= 0) {
        goto read;
    }
    total += ret;
    goto readflush;
}

int forward_flush(const directional_buffer * con) {
    int total = 0, ret;
flush:
    ensure_nonblock((ret = splice(con->pipefd[0], 0, con->paired->sockfd, 0, UINT_MAX, SPLICE_F_MORE | SPLICE_F_NONBLOCK)) != -1);
    if (ret == -1) return total;
    total += ret;
    goto flush;
}

void init_directional_buffer(directional_buffer * in_con, directional_buffer * out_con, int in_fd, int out_fd) {
    ensure(pipe(in_con->pipefd) == 0);
    ensure(pipe(out_con->pipefd) == 0);
    in_con->sockfd = in_fd;
    out_con->sockfd = out_fd;
    //link them
    in_con->paired = out_con;
    out_con->paired = in_con;
}

//will exit if the other side is already closed
void close_directional_buffer(directional_buffer * con) {
    close(con->sockfd);
    close(con->pipefd[0]);
    close(con->pipefd[1]);

    close(con->paired->sockfd);
    close(con->paired->pipefd[0]);
    close(con->paired->pipefd[1]);

    //other side is already closed
    if (!con->paired) {
        con->paired->paired = 0;
    }
    free(con);
}

//==>udp composing wrappers<==
int udp_buffer_read(udp_buffer * con) {
    socklen_t addr_len;
    int total = 0, ret;
readmsg:
    addr_len = sizeof(struct sockaddr_storage);
    ensure_nonblock((ret = recvfrom(con->sockfd, con->buffer + con->pos, BUFFER_SIZE - con->pos, 0, (struct sockaddr *)&con->addr,
                    &addr_len)) != -1);
    if (ret == -1)
        return total;
    total += ret;
    con->pos += ret;
    goto readmsg;
}

int udp_buffer_flush(udp_buffer * con) {
    int ret;
    socklen_t addr_len = sizeof(struct sockaddr_storage);
    ensure_nonblock((ret = sendto(con->sockfd, con->buffer, con->pos, 0, (struct sockaddr *)&con->next->addr, addr_len)) != -1);
    if (ret > 0)
        con->pos -= ret;
    return ret;
}

void init_udp_buffer(udp_buffer * in_con, udp_buffer * out_con) {
    in_con->pos = 0;
    out_con->pos = 0;
    in_con->next = out_con;
    out_con->next = in_con;
}

//==> forwarding rulerappers<==
void add_pairs(pairs ** restrict head, const char * restrict arg) {
    pairs * current;
    int len = strlen(arg) + 1;//+1 for null at the end
    if (*head != NULL) {
        for(current = *head; current->next != NULL; current = current->next);
        current->next = calloc(1, sizeof(pairs) + len);
        current = current->next;
    } else {
        current = calloc(1, sizeof(pairs) + len);
        *head = current;
    }

    current->addr = ((char *)current) + sizeof(pairs);

    strncpy(current->addr, arg, len);
    char * temp = strchr(current->addr, '@');

    if (temp == NULL) {
        printf("Incorrect record format for `%s`\n"
                "Allowed formats: <IP>@<listening-port> or <IP>@<listening-port>:<forwarded-port>\n", arg);
        exit(1);
    }

    current->i_port = temp + 1;
    *temp = '\0';

    temp = strchr(current->i_port, ':');
    if (temp == NULL) {
        current->o_port = current->i_port;
    } else {
        current->o_port = temp + 1;
        *temp = '\0';
    }
    current->next = NULL;
}

void print_pairs(const pairs * head) {
    for(const pairs * current = head; current != NULL; current = current->next)
        printf("link %s -> %s at %s\n", current->i_port, current->o_port, current->addr);
}

void free_pairs(pairs * restrict head) {
    if (head->next) {
        free_pairs(head->next);
    }
    free(head);
}
