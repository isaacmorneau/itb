/*
 * These wrappers are shared across multiple projects and are collected here
 * to make it easier to add to new projects and backport fixes
 *
 * use `#define ITB_IMPLEMENTATION` before including to create the implemenation
 * for example:

#include "itb.h"
#define ITB_IMPLEMENTATION
#include "itb.h"

 *
 * inspired by https://github.com/nothings/stb/, thanks Sean
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ITB_H
#define ITB_H

//==>configureable defines<==
//allow either static or extern linking
#ifdef ITB_STATIC
#define ITBDEF static
#else
#define ITBDEF extern
#endif

//allow different broadcast queue sizes
#ifndef ITB_BROADCAST_QUEUE_SIZE
#define ITB_BROADCAST_QUEUE_SIZE 16
#endif

//allow starting at different sizes
#ifndef ITB_VECTOR_INITIAL_SIZE
#define ITB_VECTOR_INITIAL_SIZE 2
#endif

#include <stdint.h>
#include <stdlib.h>

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

#define itb_release_only(expr)

#define itb_debug_only(expr) \
    do {                     \
        expr;                \
    } while (0)
#else
#define itb_release_only(expr) \
    do {                       \
        expr;                  \
    } while (0)

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

#define ITB_STRINGIFY(param) #param

//==>buffer macros <==

#ifndef ITB_BUFFER_SIZE_TYPE
#define ITB_BUFFER_SIZE_TYPE int32_t
#endif

//how long is your data
#define ITB_BUFFER_LEN(buffer) (*(ITB_BUFFER_SIZE_TYPE *)(buffer))
//how long is the actual buffer
#define ITB_BUFFER_ALLOC(buffer) (sizeof(ITB_BUFFER_SIZE_TYPE) + (*(ITB_BUFFER_SIZE_TYPE *)buffer))
//where is your data
#define ITB_BUFFER_DATA(buffer) ((void *)((uint8_t *)(buffer) + sizeof(ITB_BUFFER_SIZE_TYPE)))
//allocate a new buffer
#define ITB_BUFFER_MALLOC(buffer, size)                                 \
    do {                                                                \
        if (((buffer) = malloc((size) + sizeof(ITB_BUFFER_SIZE_TYPE)))) \
            *((ITB_BUFFER_SIZE_TYPE *)(buffer)) = (size);               \
    } while (0)
//reallocate an existing buffer
#define ITB_BUFFER_REALLOC(buffer, size)                                         \
    do {                                                                         \
        void *temp = (buffer);                                                   \
        if ((buffer) = realloc((buffer), (size) + sizeof(ITB_BUFFER_SIZE_TYPE))) \
            *((ITB_BUFFER_SIZE_TYPE *)(buffer)) = (size);                        \
    } while (0)

//==>fd ioctl wrappers<==
//the wrappers for ioctl of both sockets and the program itself
ITBDEF void itb_set_fd_limit(void);
ITBDEF void itb_set_non_blocking(int sfd);

//==>broadcast queue<==

typedef struct {
    int type;
    union {
        int flag;
        void *data;
    } extra;
} itb_broadcast_msg_t;

ITBDEF void itb_broadcast_init(void);
ITBDEF void itb_broadcast_close(void);

//blocking call, avoid use
//  ie for critical messages
ITBDEF void itb_broadcast_msg(const itb_broadcast_msg_t *msg);

//non blocking, prefer this method
ITBDEF int itb_broadcast_queue_msg(const itb_broadcast_msg_t *msg);

//handle an aditional type
//returns the type or -1 on error
ITBDEF int itb_broadcast_register_type(void);
//hook callback to type
ITBDEF int itb_broadcast_register_callback(
    int type, void (*callback)(const itb_broadcast_msg_t *msg));

//==>quick threading wrappers<==
ITBDEF pthread_t itb_quickthread(void *(func)(void *), void *param);

//==>vector<==
//TODO fully test

//default to doubling
//usually:tm: memory is cheap and mallocs are slow
//if you want something different just define your own increase pattern
#ifndef ITB_VECTOR_ENLARGE
#define ITB_VECTOR_ENLARGE(x) (x *= 2)
#endif

typedef struct {
    void *data;
    size_t size;
    size_t alloc;
    size_t _bytes_per;
} itb_vector_t;

ITBDEF int itb_vector_init(itb_vector_t *vec, size_t member_size);
ITBDEF void itb_vector_close(itb_vector_t *vec);

ITBDEF void *itb_vector_at(itb_vector_t *vec, size_t pos);
ITBDEF int itb_vector_push(itb_vector_t *vec, void *item);
ITBDEF void *itb_vector_pop(itb_vector_t *vec);
ITBDEF int itb_vector_remove_at(itb_vector_t *vec, size_t pos);

#endif //ITB_H

#ifdef ITB_IMPLEMENTATION
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

//==>fd ioctl wrappers<==
void itb_set_fd_limit(void) {
    struct rlimit lim;
    //the kernel patch that allows for RLIM_INFINITY to work breaks stuff
    //so we are restricted to finite values,
    //this was found as the exact max via trial and error
    lim.rlim_cur = (1UL << 20);
    lim.rlim_max = (1UL << 20);
    itb_ensure(setrlimit(RLIMIT_NOFILE, &lim) != -1);
}

void itb_set_non_blocking(int sfd) {
    int flags;
    itb_ensure((flags = fcntl(sfd, F_GETFL, 0)) != -1);
    flags |= O_NONBLOCK;
    itb_ensure(fcntl(sfd, F_SETFL, flags) != -1);
}

//==>broadcast queue<==

typedef struct {
    itb_broadcast_msg_t buffer[ITB_BROADCAST_QUEUE_SIZE];
    int head;
    int tail;
} itb_broadcast_msg_queue_t;

//itb_broadcast file globals
itb_broadcast_msg_queue_t queue;
sem_t itb_queue_sem;
pthread_mutex_t itb_queue_mut     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t itb_broadcast_mut = PTHREAD_MUTEX_INITIALIZER;

int itb_broadcast_total_types = 0;
//how many callbacks there are per message index pos
int *itb_broadcast_type_totals = NULL;
//the callbacks array indexed by type and type_totals
void (***itb_broadcast_callbacks)(const itb_broadcast_msg_t *msg) = NULL;

void *itb_broadcast_handler(void *unused) {
    (void)unused;
    while (1) {
        //wait for a message to be queued
        sem_wait(&itb_queue_sem);
        //dont let further circ buff modifications happen yet
        pthread_mutex_lock(&itb_queue_mut);
        //update the circ buff and consume the tail
        if (queue.tail != queue.head) {
            itb_broadcast_msg(&queue.buffer[queue.tail]);
            int next = queue.tail + 1;
            if (next == ITB_BROADCAST_QUEUE_SIZE) {
                next = 0;
            }
            queue.tail = next;
        }
        pthread_mutex_unlock(&itb_queue_mut);
    }
    return 0;
}

void itb_broadcast_init(void) {
    queue.head = 0;
    queue.tail = 0;
    itb_ensure(sem_init(&itb_queue_sem, 0, 0) != -1);
    //spin up the broadcast msg consuming thread
    pthread_t th_id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&th_id, &attr, itb_broadcast_handler, 0);
    pthread_detach(th_id);
}

void itb_broadcast_close(void) {
    sem_close(&itb_queue_sem);
    pthread_mutex_destroy(&itb_queue_mut);
    pthread_mutex_destroy(&itb_broadcast_mut);

    for (int i = 0; i < itb_broadcast_total_types; ++i) {
        free(itb_broadcast_callbacks[i]);
    }
    free(itb_broadcast_callbacks);
    free(itb_broadcast_type_totals);

    itb_broadcast_total_types = 0;
    itb_broadcast_type_totals = NULL;
    itb_broadcast_callbacks   = NULL;
}

void itb_broadcast_msg(const itb_broadcast_msg_t *restrict msg) {
    //only broadcast one at a time
    pthread_mutex_lock(&itb_broadcast_mut);
    for (int j = 0; j < itb_broadcast_type_totals[msg->type]; ++j) {
        itb_broadcast_callbacks[msg->type][j](msg);
    }
    pthread_mutex_unlock(&itb_broadcast_mut);
}

int itb_broadcast_queue_msg(const itb_broadcast_msg_t *restrict msg) {
    pthread_mutex_lock(&itb_queue_mut);
    int next = queue.head;

    if (next == ITB_BROADCAST_QUEUE_SIZE) {
        next = 0;
    }

    if (next + 1 == queue.tail) {
        pthread_mutex_unlock(&itb_queue_mut);
        return -1; //queue full
    }

    queue.buffer[next++] = *msg;
    queue.head           = next;

    pthread_mutex_unlock(&itb_queue_mut);
    sem_post(&itb_queue_sem);
    return 0; //data pushed
}

//handle an aditional type
int itb_broadcast_register_type(void) {
    pthread_mutex_lock(&itb_queue_mut);
    ++itb_broadcast_total_types;
    int *tempbtt;
    if (!(tempbtt = realloc(itb_broadcast_type_totals, itb_broadcast_total_types * sizeof(int)))) {
        pthread_mutex_unlock(&itb_queue_mut);
        return -1; //failed to realloc, OOM maybe
    }

    void (***tempbc)(const itb_broadcast_msg_t *);
    if (!(tempbc = realloc(itb_broadcast_callbacks,
              itb_broadcast_total_types * sizeof(void (**)(const itb_broadcast_msg_t *))))) {
        pthread_mutex_unlock(&itb_queue_mut);
        return -1; //failed to realloc, OOM maybe
    }

    itb_broadcast_type_totals = tempbtt;
    itb_broadcast_callbacks   = tempbc;

    //no callbacks are registererd yet
    itb_broadcast_type_totals[itb_broadcast_total_types - 1] = 0;
    //the callback buffer is uninitialized
    itb_broadcast_callbacks[itb_broadcast_total_types - 1] = 0;

    pthread_mutex_unlock(&itb_queue_mut);
    return itb_broadcast_total_types - 1;
}

//hook callback to type
int itb_broadcast_register_callback(int type, void (*callback)(const itb_broadcast_msg_t *msg)) {
    pthread_mutex_lock(&itb_queue_mut);

    if (itb_broadcast_type_totals[type]++
        == 0) { //this is the first callback, malloc the new buffer
        itb_broadcast_callbacks[type] = malloc(sizeof(void (*)(const itb_broadcast_msg_t *)));
    } else { //expand existing buffer
        void (**temp)(const itb_broadcast_msg_t *);
        if (!(temp = realloc(itb_broadcast_callbacks[type],
                  sizeof(void (*)(const itb_broadcast_msg_t *))
                      * itb_broadcast_type_totals[type]))) {
            --itb_broadcast_type_totals[type];
            pthread_mutex_unlock(&itb_queue_mut);
            return -1;
        }
        itb_broadcast_callbacks[type] = temp;
    }

    itb_broadcast_callbacks[type][itb_broadcast_type_totals[type] - 1] = callback;

    pthread_mutex_unlock(&itb_queue_mut);
    return 0;
}

pthread_t itb_quickthread(void *(func)(void *), void *param) {
    pthread_t th_id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&th_id, &attr, func, param);
    pthread_detach(th_id);
    return th_id;
}
//==>vectors<==

//typedef struct {
//    void *data;
//    size_t used;
//    size_t size;
//    size_t _bytes_per;
//} itb_vector_t;

int itb_vector_init(itb_vector_t *vec, size_t member_size) {
    vec->_bytes_per = member_size;
    vec->size       = 0;
    vec->alloc      = ITB_VECTOR_INITIAL_SIZE;

    if ((vec->data = malloc(vec->alloc * vec->_bytes_per))) {
        return 0;
    }
    return -1;
}
void itb_vector_close(itb_vector_t *vec) {
    vec->alloc      = 0;
    vec->size       = 0;
    vec->_bytes_per = 0;
    free(vec->data);
    vec->data = NULL;
}

void *itb_vector_at(itb_vector_t *vec, size_t pos) {
    if (pos >= vec->size) {
        return NULL; //check bounds
    }
    return (uint8_t *)vec->data + (pos * vec->_bytes_per);
}
int itb_vector_push(itb_vector_t *vec, void *item) {
    //TODO maybe check if the type is the size of an stdint and use int casts to avoid needing memcpy
    if (vec->size == vec->alloc) {
        ITB_VECTOR_ENLARGE(vec->alloc);
        void *temp = vec->data;
        if (!(vec->data = realloc(vec->data, vec->alloc * vec->_bytes_per))) {
            //realloc failed, better restore vec->data
            vec->data = temp;
            return 1;
        }
    }
    memcpy((uint8_t *)vec->data + vec->size * vec->_bytes_per, item, vec->_bytes_per);
    ++(vec->size);
    return 0;
}
void *itb_vector_pop(itb_vector_t *vec) {
    //return the address and adjust size but dont downsize
    return (uint8_t *)vec->data + (--(vec->size) * vec->_bytes_per);
}

int itb_vector_remove_at(itb_vector_t *vec, size_t pos) {
    if (pos >= vec->size) {
        return 1; //check bounds
    }
    if (pos == vec->size - 1) {
        //last element (just use pop)
        --(vec->size);
        return 0;
    }
    memmove((uint8_t *)vec->data + (pos * vec->_bytes_per),
        (uint8_t *)vec->data + ((pos + 1) * vec->_bytes_per), vec->_bytes_per * (vec->size - pos));
    --(vec->size);
    return 0;
}

#endif //ITB_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
