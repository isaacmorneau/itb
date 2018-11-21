#ifdef __cplusplus
extern "C" {
#endif

#ifndef ITB_UI_H
#define ITB_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

//==>configureable defines<==
//allow either static or extern linking
#ifdef ITB_STATIC
#define ITBDEF static
#else
#define ITBDEF extern
#endif

#define ITB_ANSI_COLOR_RED "\x1b[31m"
#define ITB_ANSI_COLOR_GREEN "\x1b[32m"
#define ITB_ANSI_COLOR_YELLOW "\x1b[33m"
#define ITB_ANSI_COLOR_BLUE "\x1b[34m"
#define ITB_ANSI_COLOR_MAGENTA "\x1b[35m"
#define ITB_ANSI_COLOR_CYAN "\x1b[36m"
#define ITB_ANSI_COLOR_RESET "\x1b[0m"

//==>basic nice menus<==
typedef enum itb_menu_item_type_t {
    LABEL, //text only
    CALLBACK, //run a function on press
    MENU, //switch to a submenu
    TOGGLE //toggle a flag
} itb_menu_item_type_t;

struct itb_menu_t;
typedef struct itb_menu_item_t {
    bool free_on_close;
    //name of the item
    char* label;
    //which kind and what it filters on
    //label ignores extra
    itb_menu_item_type_t type;
    union {
        void (*callback)(void);
        struct itb_menu_t* menu;
        bool* toggle;
    } extra;
} itb_menu_item_t;

typedef struct itb_menu_t {
    bool free_on_close;
    //name of the menu
    char* header;
    //how many and which items
    size_t total_items;
    itb_menu_item_t** items;
} itb_menu_t;

//sets free_on_close to false
ITBDEF int itb_menu_init(itb_menu_t* menu, const char* header);
ITBDEF void itb_menu_close(itb_menu_t* menu);

//sets free_on_close to false
ITBDEF int itb_menu_item_init(itb_menu_item_t* item, const char* text);
ITBDEF void itb_menu_item_close(itb_menu_item_t* item);

ITBDEF int itb_menu_register_item(itb_menu_t* menu, itb_menu_item_t* item);
//usage expects items terminated by a NULL
//itb_menu_register_items(menu, item1, item2, ..., NULL);
ITBDEF int itb_menu_register_items(itb_menu_t* menu, ...);

//sets free_on_close to true
ITBDEF itb_menu_item_t* itb_menu_item_label(const char* text);
ITBDEF itb_menu_item_t* itb_menu_item_callback(const char* text, void (*callback)(void));
ITBDEF itb_menu_item_t* itb_menu_item_menu(const char* text, itb_menu_t* menu);
ITBDEF itb_menu_item_t* itb_menu_item_toggle(const char* text, bool* flag);

ITBDEF void itb_menu_print(const itb_menu_t* menu);
ITBDEF void itb_menu_run(const itb_menu_t* menu);

//- errno on error or len of total read
// will clear trailing input on read, terminates with '\0' not '\n'
ITBDEF ssize_t itb_readline(uint8_t* buffer, size_t len);
#endif //ITB_UI_H
#ifdef ITB_UI_IMPLEMENTATION
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int itb_menu_init(itb_menu_t* menu, const char* header) {
    //to make sure that its as dynamic as possible just copy in the string
    //and let the caller deal with how it happens
    menu->header = malloc(strlen(header) + 1);
    if (menu->header) {
        strcpy(menu->header, header);
        menu->total_items   = 0;
        menu->items         = NULL;
        menu->free_on_close = false;
        return 0;
    }
    return 1;
}

void itb_menu_close(itb_menu_t* menu) {
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

void itb_menu_item_close(itb_menu_item_t* item) {
    if (item->type == MENU) { //might need recursion, it will free itself as needed
        itb_menu_close(item->extra.menu);
    }
    //dont need special handling just free the pointer
    if (item->free_on_close) {
        free(item);
    }
}

int itb_menu_register_item(itb_menu_t* menu, itb_menu_item_t* item) {
    ++menu->total_items;
    if (menu->items) {
        itb_menu_item_t** temp = menu->items;
        menu->items = realloc(menu->items, menu->total_items * sizeof(itb_menu_item_type_t*));
        if (!menu->items) { //realloc failed
            menu->items = temp;
            return -1;
        }
    } else {
        menu->items = malloc(sizeof(itb_menu_item_type_t*));
        if (!menu->items) { //malloc failed
            return -1;
        }
    }
    menu->items[menu->total_items - 1] = item;
    return 0;
};

//usage expects items terminated by a NULL
//itb_menu_register_items(menu, item1, item2, ..., NULL);
int itb_menu_register_items(itb_menu_t* menu, ...) {
    va_list args;
    va_start(args, menu);
    itb_menu_item_t* temp = NULL;
    while ((temp = va_arg(args, itb_menu_item_t*))) {
        if (itb_menu_register_item(menu, temp)) {
            return 1;
        }
    }
    va_end(args);
    return 0;
}

itb_menu_item_t* itb_menu_item_label(const char* text) {
    size_t len            = strlen(text) + 1;
    itb_menu_item_t* temp = malloc(sizeof(itb_menu_item_t) + len);
    if (temp) {
        temp->free_on_close = true;
        temp->label         = ((char*)temp) + sizeof(itb_menu_item_t);
        temp->type          = LABEL;
        strcpy(temp->label, text);
        return temp;
    }
    return NULL;
}

itb_menu_item_t* itb_menu_item_callback(const char* text, void (*callback)(void)) {
    size_t len            = strlen(text) + 1;
    itb_menu_item_t* temp = malloc(sizeof(itb_menu_item_t) + len);
    if (temp) {
        temp->free_on_close  = true;
        temp->label          = ((char*)temp) + sizeof(itb_menu_item_t);
        temp->extra.callback = callback;
        temp->type           = CALLBACK;
        strcpy(temp->label, text);
        return temp;
    }
    return NULL;
}

itb_menu_item_t* itb_menu_item_menu(const char* text, itb_menu_t* menu) {
    size_t len            = strlen(text) + 1;
    itb_menu_item_t* temp = malloc(sizeof(itb_menu_item_t) + len);
    if (temp) {
        temp->free_on_close = true;
        temp->label         = ((char*)temp) + sizeof(itb_menu_item_t);
        temp->extra.menu    = menu;
        temp->type          = MENU;
        strcpy(temp->label, text);
        return temp;
    }
    return NULL;
}

itb_menu_item_t* itb_menu_item_toggle(const char* text, bool* flag) {
    size_t len            = strlen(text) + 1;
    itb_menu_item_t* temp = malloc(sizeof(itb_menu_item_t) + len);
    if (temp) {
        temp->free_on_close = true;
        temp->label         = ((char*)temp) + sizeof(itb_menu_item_t);
        temp->extra.toggle  = flag;
        temp->type          = TOGGLE;
        strcpy(temp->label, text);
        return temp;
    }
    return NULL;
}

void itb_menu_print(const itb_menu_t* menu) {
    printf("<%s>\n", menu->header);
    size_t i = 0, j = 0;
    for (; i < menu->total_items; ++i) {
        if (menu->items[i]->type == LABEL) {
            printf("%s\n", menu->items[i]->label);
        } else {
            printf("[%lu] %s\n", ++j, menu->items[i]->label);
        }
    }
    printf("[%lu] exit\n", i);
}

void itb_menu_run(const itb_menu_t* menu) {
    uint8_t buffer[64];
    char *start = (char*)buffer, *end;
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
                puts("invalid input\n");
                goto invalid;
            }

            if ((size_t)sel >= menu->total_items) {
                return;
            }

            switch (menu->items[sel]->type) {
                case CALLBACK:
                    menu->items[sel]->extra.callback();
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

ssize_t itb_readline(uint8_t* buffer, size_t len) {
    ssize_t nread;

    if ((nread = read(STDIN_FILENO, buffer, len)) == -1) {
        perror("read stdin");
        if (errno == EAGAIN) { //stdin is in nonblocking mode
            return 0;
        } else {
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

#endif //ITB_UI_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
