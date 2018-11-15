#ifdef __cplusplus
extern "C" {
#endif

#ifndef ITB_UI_H
#define ITB_UI_H

#include <stdbool.h>
#include <stddef.h>

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
    for (size_t i = 0; i < menu->total_items; ++i) {
        printf("[%lu] %s\n", i, menu->items[i]->label);
    }
}
void itb_menu_run(const itb_menu_t* menu) {
    while (1) {
        itb_menu_print(menu);
        //get input do options
    }
}

#endif //ITB_UI_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
