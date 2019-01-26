#ifdef __cplusplus
extern "C" {
#endif

#ifndef ITB_UI_H
#define ITB_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <termios.h>
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

//==>ncurses like replacement<==

typedef struct itb_ui_context {
    //for returning to normal terminal settings after
    struct termios original;
    //also corresponds to bottom right position, (1,1) top left
    size_t rows;
    size_t cols;
    //current row, current col
    size_t cursor[2];
    //x*y*2
    size_t buffsize;
    //might need to increase the size for wide char later
    //TODO Unicode support, uchar.h?
    //0 - delta buffer
    //1 - last flipped
    char **doublebuffer[2];
} itb_ui_context;

//functions as init and close but also sets the terminal modes to raw and back
//
//must be called first
ITBDEF int itb_ui_start(itb_ui_context *ui_ctx);
//must be called last
ITBDEF int itb_ui_end(itb_ui_context *ui_ctx);

//swap to back double buffer to render changes
ITBDEF void itb_ui_flip(itb_ui_context *ui_ctx);

//move the cursor to the pos
ITBDEF void itb_ui_mv(itb_ui_context *ui_ctx, size_t row, size_t col);

//starts at the top left
ITBDEF void itb_ui_box(itb_ui_context *ui_ctx, size_t row, size_t col, size_t width, size_t height);

//starts at the top left
ITBDEF void itb_ui_clear(itb_ui_context *ui_ctx);

//starts at current cursor
ITBDEF void itb_ui_printf(itb_ui_context *ui_ctx, const char *fmt, ...);

#endif //ITB_UI_H
#ifdef ITB_UI_IMPLEMENTATION
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

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

//==>ncurses like replacement<==

int itb_ui_start(itb_ui_context *ui_ctx) {
    if (!isatty(STDIN_FILENO)) {
        return 1;
    }

    if (tcgetattr(STDIN_FILENO, &ui_ctx->original)) {
        return 1;
    }

    struct termios raw;

    raw = ui_ctx->original;

    //input modes - clear indicated ones giving: no break, no CR to NL,
    //no parity check, no strip char, no start/stop output (sic) control
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    //output modes - clear giving: no post processing such as NL to CR+NL
    raw.c_oflag &= ~(OPOST);

    //control modes - set 8 bit chars
    raw.c_cflag |= (CS8);

    //local modes - clear giving: echoing off, canonical off (no erase with
    //backspace, ^U,...),  no extended functions, no signal chars (^Z,^C)
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    //control chars - set return condition: min number of bytes and timer
    //raw.c_cc[VMIN]  = 5;
    //raw.c_cc[VTIME] = 8; // after 5 bytes or .8 seconds after first byte seen
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0; // immediate - anything
    //raw.c_cc[VMIN]  = 2;
    //raw.c_cc[VTIME] = 0; // after two bytes, no timer

    //raw.c_cc[VMIN]  = 0;
    //raw.c_cc[VTIME] = 8; // after a byte or .8 seconds

    // put terminal in raw mode after flushing
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {
        return 1;
    }

    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w)) {
        return 1;
    }

    ui_ctx->rows = w.ws_row;
    ui_ctx->cols = w.ws_col;

    ui_ctx->doublebuffer[0] = malloc(ui_ctx->rows * sizeof(char **));
    ui_ctx->doublebuffer[1] = malloc(ui_ctx->rows * sizeof(char **));

    for (size_t r = 0; r < ui_ctx->rows; ++r) {
        //each row should be one contigious memory segment
        ui_ctx->doublebuffer[0][r] = malloc(ui_ctx->cols);
        memset(ui_ctx->doublebuffer[0][r], ' ', ui_ctx->cols);
        ui_ctx->doublebuffer[1][r] = malloc(ui_ctx->cols);
        memset(ui_ctx->doublebuffer[1][r], ' ', ui_ctx->cols);
    }

    //clear everything and move to the top left
    //TODO check if fputs is better
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
    ui_ctx->cursor[0] = 0; //x
    ui_ctx->cursor[1] = 0; //y

    return 0;
}

int itb_ui_end(itb_ui_context *ui_ctx) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ui_ctx->original)) {
        return 1;
    }

    for (size_t r = 0; r < ui_ctx->rows; ++r) {
        free(ui_ctx->doublebuffer[0][r]);
        free(ui_ctx->doublebuffer[1][r]);
    }

    free(ui_ctx->doublebuffer[0]);
    free(ui_ctx->doublebuffer[1]);

    return 0;
}

void itb_ui_flip(itb_ui_context *ui_ctx) {
    //move top left
    itb_ui_mv(ui_ctx, 0, 0);

    bool skipped = 1;
    for (size_t r = 0; r < ui_ctx->rows; ++r) {
        size_t col   = 0;
        size_t width = 0;
        for (size_t c = 0; c < ui_ctx->cols; ++c) {
            if (ui_ctx->doublebuffer[0][r][c] != ui_ctx->doublebuffer[1][r][c]) {
                if (!width) {
                    col = c;
                }
                ++width;
            } else if (width) {
                if (skipped) {
                    itb_ui_mv(ui_ctx, r, col);
                    skipped = 0;
                }
                fwrite(ui_ctx->doublebuffer[0][r] + col, 1, width, stdout);
                width = 0;
            } else {
                skipped = 1;
            }
        }
        //catch EOL deltas
        if (width) {
            if (skipped) {
                itb_ui_mv(ui_ctx, r, col);
                skipped = 0;
            }
            fwrite(ui_ctx->doublebuffer[0][r] + col, 1, width, stdout);
            width = 0;
        } else {
            skipped = 1;
        }
        //copy display to state
        memcpy(ui_ctx->doublebuffer[1][r], ui_ctx->doublebuffer[0][r], ui_ctx->cols);
    }

    itb_ui_mv(ui_ctx, 0, 0);
    fflush(stdout);
}

void itb_ui_mv(itb_ui_context *ui_ctx, size_t row, size_t col) {
    //only update if we actually need to
    if (ui_ctx->cursor[0] != row || ui_ctx->cursor[1] != col) {
        printf("\x1b[%ld;%ldf", row, col);
        //this feels overkill
        fflush(stdout);
        ui_ctx->cursor[0] = row;
        ui_ctx->cursor[1] = col;
    } //else no change
}

void itb_ui_box(itb_ui_context *ui_ctx, size_t row, size_t col, size_t width, size_t height) {
    //if this is outside we cant render any of it
    if (row > ui_ctx->rows || col > ui_ctx->cols || !row || !col || width < 2 || height < 2) {
        return;
    }
    //1 based to 0 based
    --row;
    --col;

    char **buffer = ui_ctx->doublebuffer[0];
    //tl
    buffer[row][col] = '+';

    if (row + height < ui_ctx->rows && col + width < ui_ctx->cols) {
        //bl
        buffer[row + height - 1][col] = '+';
        //tr
        buffer[row][col + width - 1] = '+';
        //br
        buffer[row + height - 1][col + width - 1] = '+';
    } else if (col + width < ui_ctx->cols) { // tr only
        buffer[row][col + width - 1] = '+';
    } else if (row + height < ui_ctx->rows) { // bl only
        buffer[row + height][col] = '+';
    }

    //top line can at least start
    for (size_t c = col + 1; c < ui_ctx->cols && c < col + width - 1; ++c) {
        buffer[row][c] = '-';
    }

    //bottom line may be off screen
    if (row + height < ui_ctx->rows) {
        for (size_t c = col + 1; c < ui_ctx->cols && c < col + width - 1; ++c) {
            buffer[row + height - 1][c] = '-';
        }
    }

    //left line can at least start
    for (size_t r = row + 1; r < ui_ctx->rows && r < row + height - 1; ++r) {
        buffer[r][col] = '|';
    }

    //right line may be off screen
    if (col + width < ui_ctx->cols) { // tr only
        for (size_t r = row + 1; r < ui_ctx->rows && r < row + height - 1; ++r) {
            buffer[r][col + width - 1] = '|';
        }
    }
}

void itb_ui_clear(itb_ui_context *ui_ctx) {
    for (size_t r = 0; r < ui_ctx->rows; ++r) {
        memset(ui_ctx->doublebuffer[0][r], ' ', ui_ctx->cols);
    }
}

void itb_ui_printf(itb_ui_context *ui_ctx, const char *fmt, ...) {}

#endif //ITB_UI_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
