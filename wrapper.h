#ifndef WRAPPER_H
#define WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif
/*
 * These wrappers are shared across multiple projects and are collected here
 * to make it easier to add to new projects and backport fixes
 */
#include <stdlib.h>
#include <netdb.h>
#include <sys/epoll.h>

//==>assert macros<==
//these have similar functionality to assert but provide more information
#define ensure(expr)\
    do {\
        if (!(expr)) {\
            fprintf(stderr, "%s::%s::%d\n\t", __FILE__, __FUNCTION__, __LINE__);\
            perror(#expr);\
            exit(errno);\
        }\
    } while(0)

#define ensure_nonblock(expr)\
    do {\
        if (!(expr) && errno != EAGAIN) {\
            fprintf(stderr, "%s::%s::%d\n\t", __FILE__, __FUNCTION__, __LINE__);\
            perror(#expr);\
            exit(errno);\
        }\
    } while(0)

//==>fd ioctl wrappers<==
//the wrappers for ioctl of both sockets and the program itself
void set_fd_limit();
void set_non_blocking(int sfd);
void set_fast(int sfd);

//==>ip wrappers<==
//functions shared between UDP and TCP
void make_storage(struct sockaddr_storage * addr, const char * host, int port);

//==>tcp wrappers<==
//functions for setting up TCP
void set_listening(int sfd);
int make_bound_tcp(const char * port);
int make_connected(const char * address, const char * port);
int accept_blind(int sfd);
int accept_addr(int sfd, struct sockaddr_storage * addr);

//==>udp wrappers<==
//functions for setting up UDP
int make_bound_udp(int port);
int read_message(int sockfd, char * buffer, int len);
int read_message_addr(int sockfd, char * buffer, int len, struct sockaddr_storage * addr);
int read_message_port(int sockfd, char * buffer, int len, int * port);
int send_message(int sockfd, const char * buffer, int len, const struct sockaddr_storage * addr);


//==>epoll wrappers<==
//wrappers for setting up and using epoll
#define MAXEVENTS 256
//simplicity wrappers for more readable code
#define EVENT_IN(events, i) (events[i].events & EPOLLIN)
#define EVENT_ERR(events, i) (events[i].events & EPOLLERR)
#define EVENT_HUP(events, i) (events[i].events & EPOLLHUP)
#define EVENT_OUT(events, i) (events[i].events & EPOLLIN)

#define EVENT_FD(events, i) (events[i].data.fd)
#define EVENT_PTR(events, i) (events[i].data.ptr)


int make_epoll();
inline struct epoll_event * make_epoll_events() {
    return (struct epoll_event *)malloc(sizeof(struct epoll_event)*MAXEVENTS);
}
int wait_epoll(int efd, struct epoll_event * events);
int wait_epoll_timeout(int efd, struct epoll_event * events, int timeout);
int add_epoll_ptr(int efd, int ifd, void * ptr);
int add_epoll_fd(int efd, int ifd);
//common flag overrides
#define EVENT_ONLY_OUT (EPOLLOUT | EPOLLET | EPOLLEXCLUSIVE)
#define EVENT_ONLY_IN (EPOLLIN | EPOLLET | EPOLLEXCLUSIVE)
int add_epoll_ptr_flags(int efd, int ifd, void * ptr, int flags);
int add_epoll_fd_flags(int efd, int ifd, int flags);

//==>connection forwarding wrappers<==
//functions for forwarding TCP connections
struct directional_buffer;
typedef struct directional_buffer {
    int sockfd;
    int pipefd[2];
    struct directional_buffer * paired;
} directional_buffer;

int directional_echo(const directional_buffer * con);
int directional_flush(const directional_buffer * con);

void close_directional_buffer(directional_buffer * con);
void init_directional_buffer(directional_buffer * in_con, directional_buffer * out_con, int in_fd, int out_fd);

//==>udp composing wrappers<==
//functions for forwarding UDP data
#define BUFFER_SIZE 1024

struct udp_buffer;
typedef struct udp_buffer {
    struct sockaddr_storage addr;
    int pos;
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct udp_buffer * next;
} udp_buffer;

int udp_buffer_read(udp_buffer * con);
int udp_buffer_flush(udp_buffer * con);

void init_udp_buffer(udp_buffer * in_con, udp_buffer * out_con);


//==> forwarding rule wrappers<==
//functions for setting up and parsing forwarding arguments
struct pairs;
typedef struct pairs {
    char * addr;
    char * i_port;
    char * o_port;
    union {
        int fd;
        void * ptr;
    } data;
    struct pairs * next;
} pairs;

void add_pairs(pairs ** head, const char * arg);
void print_pairs(const pairs * head);
void free_pairs(pairs * head);

//==>connection echoing wrappers<==
typedef struct echoing_buffer {
    int sockfd;
    int pipefd[2];
} echoing_buffer;

void init_echoing(echoing_buffer * buf, int fd);
void close_echoing(echoing_buffer * buf);

int echoing_read(echoing_buffer * buf);
int echoing_flush(echoing_buffer * buf);

#endif

#ifdef __cplusplus
}
#endif
