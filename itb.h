/*
 * These wrappers are shared across multiple projects and are collected here
 * to make it easier to add to new projects and backport fixes
 *
 * use `#define ITB_NETWORKING_IMPLEMENTATION` before including to create the implemenation
 *
 * inspired by https://github.com/nothings/stb/, thanks Sean
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ITB_NETWORKING_H

//dont trust users, warn them if they didnt implement it by the second include
#ifndef ITB_NETWORKING_IMPLEMENTATION
#warn \
    "ITB_NETWORKING_IMPLEMENTATION still not defined by second include. Don't forget to implement."
#endif

#else //ifndef
#define ITB_NETWORKING_H

//allow either static or extern linking
#ifdef ITB_STATIC_NETWORKING
#define ITBDEF static
#else
#define ITBDEF extern
#endif

#include <netdb.h>
#include <stdlib.h>
#include <sys/epoll.h>

//==>assert macros<==
//these have similar functionality to assert but provide more information
#define itb_ensure(expr)                                                         \
    do {                                                                         \
        if (!(expr)) {                                                           \
            fprintf(stderr, "%s::%s::%d\n\t", __FILE__, __FUNCTION__, __LINE__); \
            perror(#expr);                                                       \
            exit(errno);                                                         \
        }                                                                        \
    } while (0)

#define itb_ensure_nonblock(expr)                                                \
    do {                                                                         \
        if (!(expr) && errno != EAGAIN) {                                        \
            fprintf(stderr, "%s::%s::%d\n\t", __FILE__, __FUNCTION__, __LINE__); \
            perror(#expr);                                                       \
            exit(errno);                                                         \
        }                                                                        \
    } while (0)

//==>fd ioctl wrappers<==
//the wrappers for ioctl of both sockets and the program itself
ITBDEF void itb_set_fd_limit();
ITBDEF void itb_set_non_blocking(int sfd);

//==>ip wrappers<==
//functions shared between UDP and TCP
ITBDEF void itb_make_storage(struct sockaddr_storage *addr, const char *host, int port);

//==>tcp wrappers<==
//functions for setting up TCP
ITBDEF void itb_set_listening(int sfd);
ITBDEF int itb_make_bound_tcp(const char *port);
ITBDEF int itb_make_connected(const char *address, const char *port);
ITBDEF int itb_accept_blind(int sfd);
ITBDEF int itb_accept_addr(int sfd, struct sockaddr_storage *addr);

//==>udp wrappers<==
//functions for setting up UDP
ITBDEF int itb_make_bound_udp(int port);
ITBDEF int itb_read_message(int sockfd, char *buffer, int len);
ITBDEF int itb_read_message_addr(int sockfd, char *buffer, int len, struct sockaddr_storage *addr);
ITBDEF int itb_read_message_port(int sockfd, char *buffer, int len, int *port);
ITBDEF int itb_send_message(
    int sockfd, const char *buffer, int len, const struct sockaddr_storage *addr);

//==>epoll wrappers<==
//wrappers for setting up and using epoll
#define ITB_MAXEVENTS 256
//simplicity wrappers for more readable code
#define ITB_EVENT_IN(events, i) (events[i].events & EPOLLIN)
#define ITB_EVENT_ERR(events, i) (events[i].events & EPOLLERR)
#define ITB_EVENT_HUP(events, i) (events[i].events & EPOLLHUP)
#define ITB_EVENT_OUT(events, i) (events[i].events & EPOLLIN)

#define ITB_EVENT_FD(events, i) (events[i].data.fd)
#define ITB_EVENT_PTR(events, i) (events[i].data.ptr)

ITBDEF int itb_make_epoll();
ITBDEF inline struct epoll_event *itb_make_epoll_events() {
    return (struct epoll_event *)malloc(sizeof(struct epoll_event) * MAXEVENTS);
}
ITBDEF int itb_wait_epoll(int efd, struct epoll_event *events);
ITBDEF int itb_wait_epoll_timeout(int efd, struct epoll_event *events, int timeout);
ITBDEF int itb_add_epoll_ptr(int efd, int ifd, void *ptr);
ITBDEF int itb_add_epoll_fd(int efd, int ifd);
//common flag overrides
#define ITB_EVENT_ONLY_OUT (EPOLLOUT | EPOLLET | EPOLLEXCLUSIVE)
#define ITB_EVENT_ONLY_IN (EPOLLIN | EPOLLET | EPOLLEXCLUSIVE)
ITBDEF int itb_add_epoll_ptr_flags(int efd, int ifd, void *ptr, int flags);
ITBDEF int itb_add_epoll_fd_flags(int efd, int ifd, int flags);
#endif

#ifdef ITB_NETWORKING_IMPLEMENTATION
//==>fd ioctl wrappers<==
void itb_set_fd_limit() {
    struct rlimit lim;
    //the kernel patch that allows for RLIM_INFINITY to work breaks stuff
    //so we are restricted to finite values,
    //this was found as the exact max via trial and error
    lim.rlim_cur = (1UL << 20);
    lim.rlim_max = (1UL << 20);
    ensure(setrlimit(RLIMIT_NOFILE, &lim) != -1);
}

void itb_set_non_blocking(int sfd) {
    int flags;
    ensure((flags = fcntl(sfd, F_GETFL, 0)) != -1);
    flags |= O_NONBLOCK;
    ensure(fcntl(sfd, F_SETFL, flags) != -1);
}

//==>tcp wrappers<==
void itb_set_listening(int sfd) {
    ensure(listen(sfd, SOMAXCONN) != -1);
}

int itb_make_connected(const char *address, const char *port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd = -1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_UNSPEC; // Return IPv4 and IPv6 choices
    hints.ai_socktype = SOCK_STREAM; // We want a TCP socket
    hints.ai_flags    = AI_PASSIVE; // All interfaces

    ensure(getaddrinfo(address, port, &hints, &result) == 0);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if ((sfd = socket(rp->ai_family, rp->ai_socktype | SOCK_CLOEXEC, rp->ai_protocol)) == -1) {
            continue;
        }

        if ((s = connect(sfd, rp->ai_addr, rp->ai_addrlen)) == 0) {
            break;
        }

        close(sfd);
    }

    ensure(rp != NULL);

    freeaddrinfo(result);

    set_non_blocking(sfd);
    return sfd;
}

int itb_make_bound_tcp(const char *port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd = -1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_UNSPEC; // Return IPv4 and IPv6 choices
    hints.ai_socktype = SOCK_STREAM; // We want a TCP socket
    hints.ai_flags    = AI_PASSIVE; // All interfaces

    //NULL host will bind to local
    ensure(getaddrinfo(NULL, port, &hints, &result) == 0);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if ((sfd = socket(rp->ai_family, rp->ai_socktype | SOCK_CLOEXEC, rp->ai_protocol)) == -1) {
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

int itb_accept_blind(int sfd) {
    int ret;
    ensure_nonblock((ret = accept(sfd, 0, 0)) != -1);
    return ret;
}

int itb_accept_addr(int sfd, struct sockaddr_storage *addr) {
    int ret;
    socklen_t len = sizeof(struct sockaddr_storage);
    ensure_nonblock((ret = accept(sfd, (struct sockaddr *)addr, &len)) != -1);
    return ret;
}

//==>ip wrappers<==
void itb_itb_make_storage(
    struct sockaddr_storage *restrict addr, const char *restrict host, int port) {
    struct addrinfo hints;
    struct addrinfo *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags    = AI_PASSIVE; // All interfaces

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
int itb_make_bound_udp(int port) {
    struct sockaddr_in sin;
    int sockfd;

    ensure((sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)) != -1);

    memset(&sin, 0, sizeof(sin));
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port        = htons(port);
    sin.sin_family      = AF_INET;

    int enable = 1;
    ensure(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != -1);
    ensure(bind(sockfd, (struct sockaddr *)&sin, sizeof(sin)) != -1);

    return sockfd;
}

int itb_read_message(int sockfd, char *restrict buffer, int len) {
    int total = 0, ret;
readmsg:
    ensure_nonblock((ret = recvfrom(sockfd, buffer + total, len - total, 0, NULL, NULL)) != -1);
    if (ret == -1)
        return total;
    total += ret;
    goto readmsg;
}

int itb_read_message_port(int sockfd, char *restrict buffer, int len, int *port) {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int total = 0, ret;
readmsg:
    addr_len = sizeof(struct sockaddr_storage);
    ensure_nonblock((ret = recvfrom(sockfd, buffer + total, len - total, 0,
                         (struct sockaddr *)&addr, &addr_len))
        != -1);
    if (ret == -1)
        return total;
    total += ret;
    ensure(getnameinfo((struct sockaddr *)&addr, addr_len, hbuf, sizeof(hbuf), sbuf, sizeof(hbuf),
               NI_NUMERICHOST | NI_NUMERICSERV | NI_DGRAM)
        == 0);
    *port = atoi(sbuf);
    goto readmsg;
}

int itb_read_message_addr(
    int sockfd, char *restrict buffer, int len, struct sockaddr_storage *addr) {
    socklen_t addr_len;
    int total = 0, ret;
readmsg:
    addr_len = sizeof(struct sockaddr_storage);
    ensure_nonblock(
        (ret = recvfrom(sockfd, buffer + total, len - total, 0, (struct sockaddr *)addr, &addr_len))
        != -1);
    if (ret == -1)
        return total;
    total += ret;
    goto readmsg;
}

int itb_send_message(int sockfd, const char *restrict buffer, int len,
    const struct sockaddr_storage *restrict addr) {
    int ret;
    socklen_t addr_len = sizeof(struct sockaddr_storage);
    ensure_nonblock(
        (ret = sendto(sockfd, buffer, len, 0, (struct sockaddr *)addr, addr_len)) != -1);
    return ret;
}

//==>epoll wrappers<==
int itb_make_epoll() {
    int efd;
    ensure((efd = epoll_create1(EPOLL_CLOEXEC)) != -1);
    return efd;
}

int itb_wait_epoll(int efd, struct epoll_event *restrict events) {
    int ret;
    ensure((ret = epoll_wait(efd, events, MAXEVENTS, -1)) != -1);
    return ret;
}

int itb_wait_epoll_timeout(int efd, struct epoll_event *restrict events, int timeout) {
    int ret;
    ensure((ret = epoll_wait(efd, events, MAXEVENTS, timeout)) != -1);
    return ret;
}

int itb_add_epoll_ptr(int efd, int ifd, void *ptr) {
    int ret;
    static struct epoll_event event;
    event.data.ptr = ptr;
    event.events   = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

int itb_add_epoll_ptr_flags(int efd, int ifd, void *ptr, int flags) {
    int ret;
    static struct epoll_event event;
    event.data.ptr = ptr;
    event.events   = flags;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

int itb_add_epoll_fd(int efd, int ifd) {
    int ret;
    static struct epoll_event event;
    event.data.fd = ifd;
    event.events  = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

int itb_add_epoll_fd_flags(int efd, int ifd, int flags) {
    int ret;
    static struct epoll_event event;
    event.data.fd = ifd;
    event.events  = flags;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

#endif //ITB_NETWORKING_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
