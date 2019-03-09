/*
 * origin: https://github.com/isaacmorneau/itb
 *
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
//allow static, extern, or no specifier linking
#ifndef ITBDEF
#ifdef ITB_STATIC
#define ITBDEF static
#elif ITB_EXTERN
#define ITBDEF extern
#else
#define ITBDEF
#endif
#endif

//allow different broadcast queue sizes
#ifndef ITB_BROADCAST_QUEUE_SIZE
#define ITB_BROADCAST_QUEUE_SIZE 16
#endif

//allow starting at different sizes
#ifndef ITB_VECTOR_INITIAL_SIZE
#define ITB_VECTOR_INITIAL_SIZE 2
#endif

#include <stdbool.h>
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

//==>daemon wrappers<==
ITBDEF int itb_daemonize(void);

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

//==>uri helpers<==
typedef struct {
    void *buffer;
    size_t len;
    char *prefix;
    char *host;
    char *suffix;
} itb_uri_t;

enum itb_uri_type { HOST, PREFIX_HOST, HOST_SUFFIX, PREFIX_HOST_SUFFIX, ERROR };

ITBDEF enum itb_uri_type itb_uri_parse(itb_uri_t *uri, const char *s);
ITBDEF void itb_uri_print(itb_uri_t *uri);
ITBDEF void itb_uri_close(itb_uri_t *uri);

//==>basic menu<==
typedef enum itb_menu_item_type_t {
    LABEL, //text only
    CALLBACK, //run a function on press
    MENU, //switch to a submenu
    TOGGLE //toggle a flag
} itb_menu_item_type_t;

struct itb_callback_item {
    void (*func)(void *);
    void *data;
};

struct itb_menu_t;
typedef struct itb_menu_item_t {
    bool free_on_close;
    //name of the item
    char *label;
    //which kind and what it filters on
    //label ignores extra
    itb_menu_item_type_t type;
    union {
        struct itb_callback_item *callback;
        struct itb_menu_t *menu;
        bool *toggle;
    } extra;
} itb_menu_item_t;

typedef struct itb_menu_t {
    bool free_on_close;
    //name of the menu
    char *header;
    //how many and which items
    size_t total_items;
    itb_menu_item_t **items;
    //either previous or jump to pointer
    struct itb_menu_t *stacked;
} itb_menu_t;

//sets free_on_close to false
ITBDEF int itb_menu_init(itb_menu_t *menu, const char *header);
ITBDEF void itb_menu_close(itb_menu_t *menu);

//sets free_on_close to false
ITBDEF int itb_menu_item_init(itb_menu_item_t *item, const char *text);
ITBDEF void itb_menu_item_close(itb_menu_item_t *item);

ITBDEF int itb_menu_register_item(itb_menu_t *menu, itb_menu_item_t *item);
//usage expects items terminated by a NULL
//itb_menu_register_items(menu, item1, item2, ..., NULL);
ITBDEF int itb_menu_register_items(itb_menu_t *menu, ...);

//sets free_on_close to true
ITBDEF itb_menu_item_t *itb_menu_item_label(const char *text);
ITBDEF itb_menu_item_t *itb_menu_item_callback(
    const char *text, void (*callback)(void *), void *data);
ITBDEF itb_menu_item_t *itb_menu_item_callback_ex(const char *text, void (*callback)(void *));
ITBDEF itb_menu_item_t *itb_menu_item_menu(const char *text, itb_menu_t *menu);
ITBDEF itb_menu_item_t *itb_menu_item_toggle(const char *text, bool *flag);

//display current menu position
ITBDEF void itb_menu_print(const itb_menu_t *menu);
//handle print and input loop, only good for single threaded
ITBDEF void itb_menu_run(const itb_menu_t *menu);
//assumes print was run manually
//-1 invalid, 0 fine, 1 exit
ITBDEF int itb_menu_run_once(itb_menu_t *menu, const char *line);

//- errno on error or len of total read
// will clear trailing input on read, terminates with '\0' not '\n'
ITBDEF ssize_t itb_readline(uint8_t *buffer, size_t len);

#endif //ITB_H

#ifdef ITB_IMPLEMENTATION
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
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
itb_broadcast_msg_queue_t itb_queue;
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
        //wait for a message to be itb_queued
        sem_wait(&itb_queue_sem);
        //dont let further circ buff modifications happen yet
        pthread_mutex_lock(&itb_queue_mut);
        //update the circ buff and consume the tail
        if (itb_queue.tail != itb_queue.head) {
            itb_broadcast_msg(&itb_queue.buffer[itb_queue.tail]);
            int next = itb_queue.tail + 1;
            if (next == ITB_BROADCAST_QUEUE_SIZE) {
                next = 0;
            }
            itb_queue.tail = next;
        }
        pthread_mutex_unlock(&itb_queue_mut);
    }
    return 0;
}

void itb_broadcast_init(void) {
    itb_queue.head = 0;
    itb_queue.tail = 0;
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
    int next = itb_queue.head;

    if (next == ITB_BROADCAST_QUEUE_SIZE) {
        next = 0;
    }

    if (next + 1 == itb_queue.tail) {
        pthread_mutex_unlock(&itb_queue_mut);
        return -1; //queue full
    }

    itb_queue.buffer[next++] = *msg;
    itb_queue.head           = next;

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

//==>daemon wrappers<==
int itb_daemonize(void) {
    int ret;
    if ((ret = fork()) < 0) { //>0 its good <0 its bad but we cant do anything about it
        perror("fork()");
        return errno;
        //parent error
    } else if (ret < 0) {
        //parent success
        exit(EXIT_SUCCESS);
    }
    //child

    umask(0);

    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    if (setsid() == -1) {
        perror("setsid()");
        return errno;
    }

    if (chdir("/") == -1) {
        perror("chdir()");
        return errno;
    }

    return 0;
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

//==>uri helpers<==
enum itb_uri_type itb_uri_parse(itb_uri_t *uri, const char *s) {
    if (!(uri->len = strlen(s))) {
        return ERROR;
    }

    char *prefix_eh;
    char *suffix_eh;

    prefix_eh = strstr(s, "://");
    suffix_eh = strrchr(s, ':');

    enum itb_uri_type type;

    if (!prefix_eh && !suffix_eh) { //no prefix no suffix
        type = HOST;

        ++(uri->len); //'\0'
        if (!(uri->buffer = malloc(uri->len))) {
            return ERROR;
        }

        uri->prefix = NULL;
        uri->host   = uri->buffer;
        strncpy(uri->host, s, uri->len);

        uri->suffix = NULL;
    } else if (!prefix_eh && suffix_eh) { //no prefix there is a suffix
        type = HOST_SUFFIX;

        ++(uri->len); //'\0' ':'->'\0'
        if (!(uri->buffer = malloc(uri->len))) {
            return ERROR;
        }

        size_t suffixlen = strlen(suffix_eh);
        size_t hostlen   = uri->len - suffixlen - 1; // :

        uri->prefix = NULL;

        uri->host = uri->buffer;
        strncpy(uri->host, s, hostlen);
        uri->host[hostlen] = 0;

        uri->suffix = uri->host + hostlen + 1;
        strncpy(uri->suffix, suffix_eh + 1, suffixlen);
    } else if (prefix_eh == suffix_eh) { //prefix there is no suffix
        type = PREFIX_HOST;

        --(uri->len); //'\0' '://'->'\0' 3->2
        if (!(uri->buffer = malloc(uri->len))) {
            return ERROR;
        }

        size_t prefixlen = prefix_eh - s;
        size_t hostlen   = uri->len - prefixlen - 2; // ://

        uri->prefix = uri->buffer;
        strncpy(uri->prefix, s, prefixlen);
        uri->prefix[prefixlen] = 0;

        uri->host = uri->prefix + prefixlen + 1;
        strncpy(uri->host, prefix_eh + 3, hostlen);
        uri->host[hostlen] = 0;

        uri->suffix = NULL;
    } else { //prefix and suffix
        type = PREFIX_HOST_SUFFIX;

        --(uri->len); //'\0' '://'->'\0' ':'->'\0' 4->3
        if (!(uri->buffer = malloc(uri->len))) {
            return ERROR;
        }

        size_t prefixlen = prefix_eh - s;
        size_t suffixlen = strlen(suffix_eh);
        size_t hostlen   = uri->len - prefixlen - suffixlen - 2; // :// :

        uri->prefix = uri->buffer;
        strncpy(uri->prefix, s, prefixlen);
        uri->prefix[prefixlen] = 0;

        uri->host = uri->prefix + prefixlen + 1;
        strncpy(uri->host, prefix_eh + 3, hostlen);
        uri->host[hostlen] = 0;

        uri->suffix = uri->host + hostlen + 1;
        strncpy(uri->suffix, suffix_eh + 1, suffixlen);
    }
    return type;
}

void itb_uri_print(itb_uri_t *uri) {
    if (!uri->buffer)
        return;
    if (uri->prefix)
        printf("prefix: %s\n", uri->prefix);
    if (uri->host)
        printf("host: %s\n", uri->host);
    if (uri->suffix)
        printf("suffix: %s\n", uri->suffix);
    printf("total: ");
    if (uri->prefix)
        printf("%s://", uri->prefix);
    if (uri->host)
        printf("%s", uri->host);
    if (uri->suffix)
        printf(":%s", uri->suffix);
    puts("");
}

void itb_uri_close(itb_uri_t *uri) {
    if (!uri) {
        return;
    }

    if (uri->buffer) {
        free(uri->buffer);
    }
    memset(uri, 0, sizeof(itb_uri_t));
}

//==>basic menu<==
int itb_menu_init(itb_menu_t *menu, const char *header) {
    //to make sure that its as dynamic as possible just copy in the string
    //and let the caller deal with how it happens
    menu->header = malloc(strlen(header) + 1);
    if (menu->header) {
        strcpy(menu->header, header);
        menu->total_items   = 0;
        menu->items         = NULL;
        menu->free_on_close = false;
        menu->stacked       = NULL;
        return 0;
    }
    return 1;
}

void itb_menu_close(itb_menu_t *menu) {
    //only free if there are items
    if (menu->total_items) {
        while (menu->total_items) {
            itb_menu_item_close(menu->items[--menu->total_items]);
        }
        free(menu->items);
    }

    //check if the menu itself needs freeing, includes header
    if (menu->free_on_close) {
        free(menu);
    } else { //allocated seperately and needs freeing
        free(menu->header);
    }
}

void itb_menu_item_close(itb_menu_item_t *item) {
    if (item->type == MENU) { //might need recursion, it will free itself as needed
        itb_menu_close(item->extra.menu);
    }
    //dont need special handling just free the pointer
    if (item->free_on_close) {
        free(item);
    }
}

int itb_menu_register_item(itb_menu_t *menu, itb_menu_item_t *item) {
    ++menu->total_items;
    if (menu->items) {
        itb_menu_item_t **temp = menu->items;
        menu->items = realloc(menu->items, menu->total_items * sizeof(itb_menu_item_type_t *));
        if (!menu->items) { //realloc failed
            menu->items = temp;
            return -1;
        }
    } else {
        menu->items = malloc(sizeof(itb_menu_item_type_t *));
        if (!menu->items) { //malloc failed
            return -1;
        }
    }
    menu->items[menu->total_items - 1] = item;
    return 0;
};

//usage expects items terminated by a NULL
//itb_menu_register_items(menu, item1, item2, ..., NULL);
int itb_menu_register_items(itb_menu_t *menu, ...) {
    va_list args;
    va_start(args, menu);
    itb_menu_item_t *temp = NULL;
    while ((temp = va_arg(args, itb_menu_item_t *))) {
        if (itb_menu_register_item(menu, temp)) {
            return 1;
        }
    }
    va_end(args);
    return 0;
}

itb_menu_item_t *itb_menu_item_label(const char *text) {
    size_t len            = strlen(text) + 1;
    itb_menu_item_t *temp = malloc(sizeof(itb_menu_item_t) + len);
    if (temp) {
        temp->free_on_close = true;
        temp->label         = (char *)(temp + 1);
        temp->type          = LABEL;
        strcpy(temp->label, text);
        return temp;
    }
    return NULL;
}

itb_menu_item_t *itb_menu_item_callback(const char *text, void (*callback)(void *), void *data) {
    size_t len = strlen(text) + 1;
    itb_menu_item_t *temp
        = malloc(sizeof(itb_menu_item_t) + sizeof(struct itb_callback_item) + len);
    if (temp) {
        temp->free_on_close        = true;
        temp->extra.callback       = (struct itb_callback_item *)(temp + 1);
        temp->extra.callback->func = callback;
        temp->extra.callback->data = data;
        temp->label                = (char *)(temp->extra.callback + 1);
        temp->type                 = CALLBACK;
        strcpy(temp->label, text);
        return temp;
    }
    return NULL;
}

itb_menu_item_t *itb_menu_item_menu(const char *text, itb_menu_t *menu) {
    size_t len            = strlen(text) + 1;
    itb_menu_item_t *temp = malloc(sizeof(itb_menu_item_t) + len);
    if (temp) {
        temp->free_on_close = true;
        temp->label         = (char *)(temp + 1);
        temp->extra.menu    = menu;
        temp->type          = MENU;
        strcpy(temp->label, text);
        return temp;
    }
    return NULL;
}

itb_menu_item_t *itb_menu_item_toggle(const char *text, bool *flag) {
    size_t len            = strlen(text) + 1;
    itb_menu_item_t *temp = malloc(sizeof(itb_menu_item_t) + len);
    if (temp) {
        temp->free_on_close = true;
        temp->label         = (char *)(temp + 1);
        temp->extra.toggle  = flag;
        temp->type          = TOGGLE;
        strcpy(temp->label, text);
        return temp;
    }
    return NULL;
}

void itb_menu_print(const itb_menu_t *menu) {
    bool s = false;
    if (menu->stacked) {
        s    = true;
        menu = menu->stacked;
    }
    printf("<%s>\n", menu->header);
    size_t i = 0, j = 0;
    for (; i < menu->total_items; ++i) {
        if (menu->items[i]->type == LABEL) {
            printf("%s\n", menu->items[i]->label);
        } else {
            printf("[%lu] %s\n", ++j, menu->items[i]->label);
        }
    }

    if (s) {
        printf("[%lu] back\n", ++j);
    } else {
        printf("[%lu] exit\n", ++j);
    }
}

void itb_menu_run(const itb_menu_t *menu) {
    uint8_t buffer[64];
    char *start = (char *)buffer, *end;
    ssize_t nread, sel = 0;
    while (1) {
        itb_menu_print(menu);
    invalid:
        printf("> ");
        fflush(stdout);
        if ((nread = itb_readline(buffer, 64)) > 0) {
            sel = strtoll(start, &end, 10);
            sel -= 1; //ui uses 1 based for number row but internally we want it zero based

            //skip labels
            for (ssize_t i = sel; i < (ssize_t)menu->total_items && i >= 0; --i) {
                if (menu->items[i]->type == LABEL) {
                    ++sel;
                }
            }

            if (start == end || sel < 0
                || (size_t)sel > menu->total_items) { //is an int and in the right range
                puts("invalid input");
                goto invalid;
            }

            if ((size_t)sel == menu->total_items) {
                return;
            }

            switch (menu->items[sel]->type) {
                case CALLBACK:
                    menu->items[sel]->extra.callback->func(menu->items[sel]->extra.callback->data);
                    break;
                case MENU:
                    itb_menu_run(menu->items[sel]->extra.menu);
                    break;
                case TOGGLE:
                    (*menu->items[sel]->extra.toggle) = !(*menu->items[sel]->extra.toggle);
                    break;
                default:
                    break;
            }
        } else { //^D probably, exit out
            return;
        }
    }
}

int itb_menu_run_once(itb_menu_t *menu, const char *line) {
    itb_menu_t *menu_local = menu;

    if (menu->stacked) { //return to last position if set
        menu = menu->stacked;
    }
    char *end;
    ssize_t sel = 0;

    int ret = 0;
    if (strlen(line) > 0) {
        sel = strtoll(line, &end, 10);
        sel -= 1; //ui uses 1 based for number row but internally we want it zero based

        //skip labels
        for (ssize_t i = sel; i < (ssize_t)menu->total_items && i >= 0; --i) {
            if (menu->items[i]->type == LABEL) {
                ++sel;
            }
        }

        if (line == end || sel < 0
            || (size_t)sel > menu->total_items) { //is an int and in the right range
            puts("invalid input");
            return -1;
        }

        if ((size_t)sel < menu->total_items) {
            switch (menu->items[sel]->type) {
                case CALLBACK:
                    menu->items[sel]->extra.callback->func(menu->items[sel]->extra.callback->data);
                    break;
                case MENU:
                    //set the return point to the sub menu
                    menu->items[sel]->extra.menu->stacked = menu;
                    //set the last position
                    menu_local->stacked = menu->items[sel]->extra.menu;
                    break;
                case TOGGLE:
                    (*menu->items[sel]->extra.toggle) = !(*menu->items[sel]->extra.toggle);
                    break;
                default:
                    break;
            }
            return 0;
        } else {
            ret = 1;
        }
        //else exit/back requested
    }

    //^D or exit menu option, exit out
    if (menu_local->stacked) { //there was a return set
        if (menu->stacked != menu_local->stacked) { //and it wasnt the caller
            //move up a level
            menu_local->stacked = menu->stacked;
        } else {
            //top of stack reached
            menu_local->stacked = NULL;
            ret                 = 1;
        }
    }
    return ret;
}

ssize_t itb_readline(uint8_t *buffer, size_t len) {
    ssize_t nread;

    if ((nread = read(STDIN_FILENO, buffer, len)) == -1) {
        if (errno == EAGAIN) { //stdin is in nonblocking mode
            return 0;
        } else {
            perror("readline stdin");
            return -errno;
        }
    }

    if (!nread) {
        return 0;
    }

    if (nread == (ssize_t)len) { // full read: either exact or trailing input
        if (buffer[nread - 1] != '\n') { //trailing input
            uint8_t tmp[256];
            ssize_t tmpread;
            //wipe out trailing input
            do {
                tmpread = read(STDIN_FILENO, tmp, 256);
                //if its negative its an error so quit
                //if its exact theres either more to read or it was a line that fit perfectly and would end in '\n'
            } while (tmpread != -1 && tmpread == 256 && tmp[tmpread - 1] != '\n');
        }
        //} else { exact match fall through and replace the '\n' with a '\0'
    }

    buffer[nread - 1] = '\0'; //replace '\n' with '\0' or cuts out input thats too long

    return nread;
}


#endif //ITB_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
