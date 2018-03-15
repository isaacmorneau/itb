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

//==>tcp wrappers<==
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

//==>udp wrappers<==
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

int make_bound_udp(int port) {
    struct sockaddr_in sin;
    int sockfd;

    ensure((sockfd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0)) == -1);

    memset(&sin, 0, sizeof(sin));
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    sin.sin_family = AF_INET;

    ensure(bind(sockfd, (struct sockaddr *) &sin, sizeof(sin)) == -1);

    return sockfd;
}

int read_message(int sockfd, char * restrict buffer, int len) {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int total = 0, ret;
readmsg:
    addr_len = sizeof(struct sockaddr_storage);
    ensure_nonblock((ret = recvfrom(sockfd, buffer, len, 0, (struct sockaddr *)&addr,
                    &addr_len)) != -1);
    if (ret == -1)
        return total;
    total += ret;
    ensure(getnameinfo((struct sockaddr *)&addr, addr_len, hbuf, sizeof(hbuf), sbuf, sizeof(hbuf), NI_NUMERICHOST|NI_NUMERICSERV|NI_DGRAM) == 0);
    printf("read %d from %s:%s\n", ret, hbuf, sbuf);
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

int add_epoll_ptr(int efd, int ifd, void * ptr) {
    int ret;
    static struct epoll_event event;
    event.data.ptr = ptr;
    event.events = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
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

