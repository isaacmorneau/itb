#ifdef __cplusplus
extern "C" {
#endif

#ifndef ITB_NET_H
#define ITB_NET_H

//==>configureable defines<==
//allow either static or extern linking
#ifdef ITB_STATIC
#define ITBDEF static
#else
#define ITBDEF extern
#endif

#include <errno.h>
#include <netdb.h>
#include <sys/epoll.h>

//==>assert macros<==
#ifndef ITB_ASSERTS
#define ITB_ASSERTS
#ifndef NDEBUG
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
#define itb_debug_only(expr) \
    do {                     \
        expr;                \
    } while (0)
#else
#define itb_debug_only(expr)
#define itb_ensure(expr) \
    do {                 \
        if (!(expr)) {   \
            exit(errno); \
        }                \
    } while (0)

#define itb_ensure_nonblock(expr)         \
    do {                                  \
        if (!(expr) && errno != EAGAIN) { \
            exit(errno);                  \
        }                                 \
    } while (0)
#endif
#endif //ITB_ASSERTS

//==>extra wrappers<==
ITBDEF int is_little_endian();
ITBDEF int is_big_endian();

//==>ip wrappers<==
//functions shared between UDP and TCP
ITBDEF void itb_make_storage(struct sockaddr_storage *addr, const char *host, int port);
ITBDEF void itb_print_addr(char **buff, struct sockaddr_storage *addr);

//==>tcp wrappers<==
//functions for setting up TCP
ITBDEF void itb_set_listening(int sfd);
ITBDEF int itb_make_bound_tcp(const char *port);
ITBDEF int itb_make_tcp();
ITBDEF int itb_make_connected(const char *address, const char *port);
ITBDEF int itb_accept_blind(int sfd);
ITBDEF int itb_accept_addr(int sfd, struct sockaddr_storage *addr);
ITBDEF int itb_recv(int sockfd, uint8_t *buffer, int len);
ITBDEF int itb_send(int sockfd, const uint8_t *buffer, int len);

//==>unix wrappers<==
ITBDEF int itb_make_bound_unix(const char *path);
ITBDEF int itb_make_connected_unix(const char *path);

//==>udp wrappers<==
//functions for setting up UDP
ITBDEF int itb_make_bound_udp(int port);
ITBDEF int itb_make_udp();
ITBDEF int itb_read_message(int sockfd, uint8_t *buffer, int len);
ITBDEF int itb_read_message_addr(
    int sockfd, uint8_t *buffer, int len, struct sockaddr_storage *addr);
ITBDEF int itb_read_message_port(int sockfd, uint8_t *buffer, int len, int *port);
ITBDEF int itb_send_message(
    int sockfd, const uint8_t *buffer, int len, const struct sockaddr_storage *addr);

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
ITBDEF struct epoll_event *itb_make_epoll_events();
ITBDEF int itb_wait_epoll(int efd, struct epoll_event *events);
ITBDEF int itb_wait_epoll_timeout(int efd, struct epoll_event *events, int timeout);
ITBDEF int itb_add_epoll_ptr(int efd, int ifd, void *ptr);
ITBDEF int itb_add_epoll_fd(int efd, int ifd);
//common flag overrides
#define ITB_EVENT_ONLY_OUT (EPOLLOUT | EPOLLET | EPOLLEXCLUSIVE)
#define ITB_EVENT_ONLY_IN (EPOLLIN | EPOLLET | EPOLLEXCLUSIVE)
ITBDEF int itb_add_epoll_ptr_flags(int efd, int ifd, void *ptr, int flags);
ITBDEF int itb_add_epoll_fd_flags(int efd, int ifd, int flags);

//so you dont need to link mbedtls but hey if you do, have some wrappers
#ifdef ITB_SSL_ADDITIONS

#include "mbedtls/config.h"
//TODO double check the config is sane

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
typedef struct {
    mbedtls_net_context socket;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
} itb_ssl_conn_t;

//tls1.1+ is enforced
ITBDEF int itb_ssl_init(itb_ssl_conn_t *conn, const char *host);
ITBDEF void itb_ssl_cleanup(itb_ssl_conn_t *conn);
//to allow easy changing later, assume conn is an itb_ssl_conn_t*
#define itb_ssl_write(conn, buf, len) mbedtls_ssl_write(&(conn)->ssl, buf, len)
#define itb_ssl_read(conn, buf, len) mbedtls_ssl_read(&(conn)->ssl, buf, len)

#endif // ITB_SSL_ADDITIONS

#endif //ITB_NET_H
#ifdef ITB_NET_IMPLEMENTATION
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

//==>extra wrappers<==
int is_little_endian() {
    int le = 1;
    return *(char *)&le == 1; //they are the same, its little endian
}

int is_big_endian() {
    int be = 1;
    return *(char *)&be != 1; //they are different, its big endian
}

//==>tcp wrappers<==
void itb_set_listening(int sfd) {
    itb_ensure(listen(sfd, SOMAXCONN) != -1);
}

int itb_make_connected(const char *address, const char *port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd = -1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_UNSPEC; // Return IPv4 and IPv6 choices
    hints.ai_socktype = SOCK_STREAM; // We want a TCP socket
    hints.ai_flags    = AI_PASSIVE; // All interfaces

    itb_ensure(getaddrinfo(address, port, &hints, &result) == 0);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if ((sfd = socket(rp->ai_family, rp->ai_socktype | SOCK_CLOEXEC, rp->ai_protocol)) == -1) {
            continue;
        }

        if ((s = connect(sfd, rp->ai_addr, rp->ai_addrlen)) == 0) {
            break;
        }

        close(sfd);
    }

    itb_ensure(rp != NULL);

    freeaddrinfo(result);

    itb_set_non_blocking(sfd);
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
    itb_ensure(getaddrinfo(NULL, port, &hints, &result) == 0);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if ((sfd = socket(rp->ai_family, rp->ai_socktype | SOCK_CLOEXEC, rp->ai_protocol)) == -1) {
            continue;
        }

        int enable = 1;
        itb_ensure(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != -1);

        if (!bind(sfd, rp->ai_addr, rp->ai_addrlen)) {
            //we managed to bind successfully
            break;
        }

        close(sfd);
    }

    itb_ensure(rp != NULL);

    freeaddrinfo(result);

    itb_set_non_blocking(sfd);
    return sfd;
}

int itb_make_tcp(void) {
    int sfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    return sfd;
}

int itb_accept_blind(int sfd) {
    int ret;
    itb_ensure_nonblock((ret = accept(sfd, 0, 0)) != -1);
    return ret;
}

int itb_accept_addr(int sfd, struct sockaddr_storage *addr) {
    int ret;
    socklen_t len = sizeof(struct sockaddr_storage);
    itb_ensure_nonblock((ret = accept(sfd, (struct sockaddr *)addr, &len)) != -1);
    return ret;
}

int itb_recv(int sockfd, uint8_t *restrict buffer, int len) {
    int total = 0, ret;
readmsg:
    itb_ensure_nonblock((ret = recv(sockfd, buffer + total, len - total, 0)) != -1);
    if (ret <= 0)
        return total;
    total += ret;
    goto readmsg;
}

int itb_send(int sockfd, const uint8_t *restrict buffer, int len) {
    int ret;
    itb_ensure_nonblock((ret = send(sockfd, buffer, len, 0)) != -1);
    return ret;
}

//==>unix wrappers<==
int itb_make_bound_unix(const char *path) {
    int sfd;
    itb_ensure(sfd = socket(AF_UNIX, SOCK_STREAM, 0));

    struct sockaddr_un su;
    memset(&su, 0, sizeof(struct sockaddr_un));
    su.sun_family = AF_UNIX;
    strcpy(su.sun_path, path);

    unlink(path);
    itb_ensure(bind(sfd, (struct sockaddr *)&su, sizeof(struct sockaddr_un)) != -1);

    return sfd;
}

int itb_make_connected_unix(const char *path) {
    int sfd;
    itb_ensure(sfd = socket(AF_UNIX, SOCK_STREAM, 0));

    struct sockaddr_un su;
    memset(&su, 0, sizeof(struct sockaddr_un));
    su.sun_family = AF_UNIX;
    strcpy(su.sun_path, path);

    itb_ensure(connect(sfd, (struct sockaddr *)&su, sizeof(struct sockaddr_un)) != -1);

    return sfd;
}

//==>ip wrappers<==
void itb_make_storage(struct sockaddr_storage *restrict addr, const char *restrict host, int port) {
    struct addrinfo hints;
    struct addrinfo *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags    = AI_PASSIVE; // All interfaces

    //null the service as it only accepts strings and we have the port already
    itb_ensure(getaddrinfo(host, NULL, &hints, &rp) == 0);

    //assuming the first result returned will be correct
    //TODO find a way to check
    itb_ensure(rp);

    //add the port manually
    if (rp->ai_family == AF_INET) {
        ((struct sockaddr_in *)rp->ai_addr)->sin_port = htons(port);
    } else if (rp->ai_family == AF_INET6) {
        ((struct sockaddr_in6 *)rp->ai_addr)->sin6_port = htons(port);
    }

    memcpy(addr, rp->ai_addr, rp->ai_addrlen);

    freeaddrinfo(rp);
}

void itb_print_addr(char **buff, struct sockaddr_storage *addr) {
    if (((struct sockaddr *)addr)->sa_family == AF_INET) {
        if (*buff || (*buff = malloc(INET_ADDRSTRLEN)) {
            inet_ntop(AF_INET, addr, *buff, INET_ADDRSTRLEN);
        }
    } else {
        if (*buff || (*buff = malloc(INET6_ADDRSTRLEN)) {
            inet_ntop(AF_INET6, addr, *buff, INET6_ADDRSTRLEN);
        }
    }
}

//==>udp wrappers<==
int itb_make_bound_udp(int port) {
    struct sockaddr_in sin;
    int sockfd;

    itb_ensure((sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)) != -1);

    memset(&sin, 0, sizeof(sin));
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port        = htons(port);
    sin.sin_family      = AF_INET;

    int enable = 1;
    itb_ensure(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != -1);
    itb_ensure(bind(sockfd, (struct sockaddr *)&sin, sizeof(sin)) != -1);

    return sockfd;
}

int itb_make_udp(void) {
    int sfd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    return sfd;
}

int itb_read_message(int sockfd, uint8_t *restrict buffer, int len) {
    int total = 0, ret;
readmsg:
    itb_ensure_nonblock((ret = recvfrom(sockfd, buffer + total, len - total, 0, NULL, NULL)) != -1);
    if (ret == -1)
        return total;
    total += ret;
    goto readmsg;
}

int itb_read_message_port(int sockfd, uint8_t *restrict buffer, int len, int *restrict port) {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int total = 0, ret;
readmsg:
    addr_len = sizeof(struct sockaddr_storage);
    itb_ensure_nonblock((ret = recvfrom(sockfd, buffer + total, len - total, 0,
                             (struct sockaddr *)&addr, &addr_len))
        != -1);
    if (ret == -1)
        return total;
    total += ret;
    itb_ensure(getnameinfo((struct sockaddr *)&addr, addr_len, hbuf, sizeof(hbuf), sbuf,
                   sizeof(hbuf), NI_NUMERICHOST | NI_NUMERICSERV | NI_DGRAM)
        == 0);
    *port = atoi(sbuf);
    goto readmsg;
}

int itb_read_message_addr(
    int sockfd, uint8_t *restrict buffer, int len, struct sockaddr_storage *addr) {
    socklen_t addr_len;
    int total = 0, ret;
readmsg:
    addr_len = sizeof(struct sockaddr_storage);
    itb_ensure_nonblock(
        (ret = recvfrom(sockfd, buffer + total, len - total, 0, (struct sockaddr *)addr, &addr_len))
        != -1);
    if (ret == -1)
        return total;
    total += ret;
    goto readmsg;
}

int itb_send_message(int sockfd, const uint8_t *restrict buffer, int len,
    const struct sockaddr_storage *restrict addr) {
    int ret;
    socklen_t addr_len = sizeof(struct sockaddr_storage);
    itb_ensure_nonblock(
        (ret = sendto(sockfd, buffer, len, 0, (struct sockaddr *)addr, addr_len)) != -1);
    return ret;
}

//==>epoll wrappers<==
struct epoll_event *itb_make_epoll_events() {
    return (struct epoll_event *)malloc(sizeof(struct epoll_event) * ITB_MAXEVENTS);
}
int itb_make_epoll(void) {
    int efd;
    itb_ensure((efd = epoll_create1(EPOLL_CLOEXEC)) != -1);
    return efd;
}

int itb_wait_epoll(int efd, struct epoll_event *restrict events) {
    int ret;
    if ((ret = epoll_wait(efd, events, ITB_MAXEVENTS, -1)) == -1) {
        if (errno == EINTR) {
            return 0;
        }
        return -1;
    }
    return ret;
}

int itb_wait_epoll_timeout(int efd, struct epoll_event *restrict events, int timeout) {
    int ret;
    itb_ensure((ret = epoll_wait(efd, events, ITB_MAXEVENTS, timeout)) != -1);
    return ret;
}

int itb_add_epoll_ptr(int efd, int ifd, void *ptr) {
    int ret;
    static struct epoll_event event;
    event.data.ptr = ptr;
    event.events   = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    itb_ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

int itb_add_epoll_ptr_flags(int efd, int ifd, void *ptr, int flags) {
    int ret;
    static struct epoll_event event;
    event.data.ptr = ptr;
    event.events   = flags;
    itb_ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

int itb_add_epoll_fd(int efd, int ifd) {
    int ret;
    static struct epoll_event event;
    event.data.fd = ifd;
    event.events  = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    itb_ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

int itb_add_epoll_fd_flags(int efd, int ifd, int flags) {
    int ret;
    static struct epoll_event event;
    event.data.fd = ifd;
    event.events  = flags;
    itb_ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

#ifdef ITB_SSL_ADDITIONS

int itb_ssl_init(itb_ssl_conn_t *conn, const char *host) {
    mbedtls_net_init(&conn->socket);
    mbedtls_ssl_init(&conn->ssl);
    mbedtls_ssl_config_init(&conn->conf);
    mbedtls_x509_crt_init(&conn->cacert);
    mbedtls_ctr_drbg_init(&conn->ctr_drbg);

    mbedtls_entropy_init(&conn->entropy);
    if (mbedtls_ctr_drbg_seed(&conn->ctr_drbg, mbedtls_entropy_func, &conn->entropy, NULL, 0)
        != 0) {
        return 1;
    }

    if (mbedtls_x509_crt_parse_path(&conn->cacert, "/etc/ssl/certs/") < 0) {
        return 1;
    }

    if (mbedtls_net_connect(&conn->socket, host, "443", MBEDTLS_NET_PROTO_TCP)) {
        return 1;
    }

    if (mbedtls_ssl_config_defaults(&conn->conf, MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) {
        return 1;
    }

    mbedtls_ssl_conf_authmode(&conn->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&conn->conf, &conn->cacert, NULL);
    mbedtls_ssl_conf_rng(&conn->conf, mbedtls_ctr_drbg_random, &conn->ctr_drbg);
    //mbedtls_ssl_conf_dbg(&conn->conf, debug_print, stdout);

    if (mbedtls_ssl_setup(&conn->ssl, &conn->conf)) {
        return 1;
    }

    if (mbedtls_ssl_set_hostname(&conn->ssl, host)) {
        return 1;
    }

    mbedtls_ssl_set_bio(&conn->ssl, &conn->socket, mbedtls_net_send, mbedtls_net_recv, NULL);

    if (mbedtls_ssl_handshake(&conn->ssl)) {
        return 1;
    }

    if (mbedtls_ssl_get_verify_result(&conn->ssl)) {
        return 1;
    }

    return 0;
}

void itb_ssl_cleanup(itb_ssl_conn_t *conn) {
    mbedtls_ssl_close_notify(&conn->ssl);
    mbedtls_net_free(&conn->socket);
    mbedtls_x509_crt_free(&conn->cacert);
    mbedtls_ssl_free(&conn->ssl);
    mbedtls_ssl_config_free(&conn->conf);
    mbedtls_ctr_drbg_free(&conn->ctr_drbg);
    mbedtls_entropy_free(&conn->entropy);
}

#endif // ITB_SSL_ADDITIONS

#endif //ITB_NET_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
