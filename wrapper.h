#ifndef WRAPPER_H
#define WRAPPER_H

#include <stdlib.h>
#include <netdb.h>
#include <sys/epoll.h>

//==>assert macros<==
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
void set_fd_limit();
void set_non_blocking(int sfd);

//==>ip wrappers<==
void make_storage(struct sockaddr_storage * addr, const char * host, int port);

//==>tcp wrappers<==
void set_listening(int sfd);
int make_bound_tcp(const char * port);
int make_connected(const char * address, const char * port);

//==>udp wrappers<==
int make_bound_udp(int port);
int read_message(int sockfd, char * buffer, int len);
int send_message(int sockfd, const char * buffer, int len, const struct sockaddr_storage * addr);


//==>epoll wrappers<==
#define MAXEVENTS 256
//simplicity wrappers
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


//==>connection forwarding wrappers<==
struct directional_buffer;
typedef struct directional_buffer {
    int sockfd;
    int pipefd[2];
    struct directional_buffer * paired;
} directional_buffer;

int forward_read(const directional_buffer * con);
int forward_flush(const directional_buffer * con);

void close_directional_buffer(directional_buffer * con);
void init_directional_buffer(directional_buffer * in_con, directional_buffer * out_con, int in_fd, int out_fd);

#endif
