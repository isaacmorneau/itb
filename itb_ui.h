/*
 * origin: https://github.com/isaacmorneau/itb
 *
 * These wrappers are shared across multiple projects and are collected here
 * to make it easier to add to new projects and backport fixes
 *
 * #TODO benchmark this against ncurses
 *
 * use `#define ITB_UI_IMPLEMENTATION` before including to create the implemenation
 * for example:

#define ITB_UI_IMPLEMENTATION
#include "itb_ui.h"

 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ITB_UI_H
#define ITB_UI_H

#include <locale.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <termios.h>
#include <unistd.h>

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
//default it to use unicode
//use ascii only with
//#define ITB_UI_UNICODE 0
//
//from testing the relative performance was found to be as follows for a large window
//real 0m1.604s unicode
//real 0m0.884s ascii
#ifndef ITB_UI_UNICODE
#define ITB_UI_UNICODE 1
#endif

//==>ncurses like replacement<==

//handle the correct functions for unicode vs regular
#if ITB_UI_UNICODE
#include <wchar.h>
#define __ITB_TEXT(x) L##x
#define ITB_FPRINTF fwprintf
#define ITB_SPRINTF vswprintf
#define ITB_MEMCPY wmemcpy
#define ITB_MEMSET wmemset
#define ITB_CHAR wchar_t
#define ITB_STRCMP wcsncmp
#define ITB_SEL(c, u) u
#else
#define __ITB_TEXT(x) x
#define ITB_FPRINTF fprintf
#define ITB_SPRINTF vsnprintf
#define ITB_MEMCPY memcpy
#define ITB_MEMSET memset
#define ITB_CHAR char
#define ITB_STRCMP strncmp
#define ITB_SEL(c, u) c
#endif

//color escape string literal builder
#define __ITB_COLOR_CONV(x) #x
#define ITB_COLOR(fg, bg) __ITB_TEXT("\x1b[3" __ITB_COLOR_CONV(fg) ";4" __ITB_COLOR_CONV(bg) "m")
#define ITB_COLOR_FG(fg) __ITB_TEXT("\x1b[3" __ITB_COLOR_CONV(fg) "m")
#define ITB_COLOR_BG(bg) __ITB_TEXT("\x1b[4" __ITB_COLOR_CONV(bg) "m")

#define ITB_BLACK 0
#define ITB_RED 1
#define ITB_GREEN 2
#define ITB_YELLOW 3
#define ITB_BLUE 4
#define ITB_MAGENTA 5
#define ITB_CYAN 6
#define ITB_WHITE 7

#define ITB_RESET __ITB_TEXT("\x1b[0m")

//use this to wrap strings to automatically add long string L constants as needed
#define ITB_T(x) __ITB_TEXT(x)

//to keep track of what color modes are set
//#TODO support true color
typedef union {
    struct {
        int8_t fg;
        int8_t bg;
    } set;
    //signed so you can set the flags to -1 as 0 is a valid flag
    int16_t flags;
} itb_color_mode;

//rows and columns are 1 indexed while buffers are 0 indexed these also convert between the two

//get the correct index in the flat buffers such as buffer and color_buffer
#define ITB_UI_RC_IDX(ctx, row, col, idx)          \
    do {                                           \
        idx = ((row)-1) * (ctx)->cols + ((col)-1); \
    } while (0);
//convert an idx back to row and column
#define ITB_UI_IDX_RC(ctx, idx, row, col) \
    do {                                  \
        col = (idx) % (ctx)->cols + 1;    \
        row = (idx) / ((ctx)->cols) + 1;  \
    } while (0);

//make a define to force inlining
#define ITB_UI_UPDATE_DIRTY(ctx, minidx, maxidx) \
    do {                                         \
        if (!(ctx)->is_dirty) {                  \
            (ctx)->dirty_start = (minidx);       \
            (ctx)->dirty_end   = (maxidx);       \
            (ctx)->is_dirty    = true;           \
        } else {                                 \
            if ((ctx)->dirty_start > (minidx)) { \
                (ctx)->dirty_start = (minidx);   \
            }                                    \
                                                 \
            if ((ctx)->dirty_end < (maxidx)) {   \
                (ctx)->dirty_end = (maxidx);     \
            }                                    \
        }                                        \
    } while (0);

//organized in decending order of size to minimize padding
typedef struct itb_ui_context {
    //for returning to normal terminal settings after
    struct termios original;
    //also corresponds to bottom right position, (1,1) top left
    const size_t rows;
    const size_t cols;
    //current row, current col
    size_t cursor[2];

    //start and end of affected index
    size_t dirty_start;
    size_t dirty_end;

    //0 - delta buffer
    //1 - last flipped
    //flattened grid of what color mode is set rn
    //[row * col]
    itb_color_mode *color_buffer[2];
    //[row * col]
    ITB_CHAR *buffer[2];
    //current painting color
    itb_color_mode current_color;
    //last painted color
    //itb_color_mode last_color;
    bool cursor_visible;
    //paint needed
    bool is_dirty;
} itb_ui_context;

//
typedef struct itb_ui_stash {
    ITB_CHAR *buffer;
    itb_color_mode *colors;
} itb_ui_stash;

//functions as init and close but also sets the terminal modes to raw and back
//must be called first
//this should be set before any printing happens in the program
ITBDEF int itb_ui_start(itb_ui_context *ctx);
//must be called last
ITBDEF int itb_ui_end(itb_ui_context *ctx);

//wipe the screen and the colors
ITBDEF void itb_ui_clear(itb_ui_context *ctx);

//render the scene, fast and prefered
ITBDEF void itb_ui_flip(itb_ui_context *ctx);

//move the cursor
ITBDEF void itb_ui_mv(itb_ui_context *ctx, size_t row, size_t col);
//hide the cursor
ITBDEF void itb_ui_hide(itb_ui_context *ctx);
//show the cursor
ITBDEF void itb_ui_show(itb_ui_context *ctx);

//set the current drawing color
//pass NULL for mode to rest
ITBDEF void itb_ui_color(itb_ui_context *ctx, itb_color_mode *mode);

//starts at the top left
ITBDEF void itb_ui_box(itb_ui_context *ctx, size_t row, size_t col, size_t width, size_t height);

//starts at row and col specified
ITBDEF int itb_ui_printf(itb_ui_context *ctx, size_t row, size_t col, const ITB_CHAR *fmt, ...);
ITBDEF int itb_ui_strcpy(
    itb_ui_context *ctx, size_t row, size_t col, const ITB_CHAR *str, size_t len);

//initializes a stash for a given context
ITBDEF int itb_ui_stash_init(const itb_ui_context *ctx, itb_ui_stash *stash);
//copies current ui into the stash
ITBDEF void itb_ui_stash_copy(const itb_ui_context *ctx, itb_ui_stash *stash);
//pastes stash onto current ui
ITBDEF void itb_ui_stash_paste(itb_ui_context *ctx, itb_ui_stash *stash);
//cleanup resources
ITBDEF void itb_ui_stash_close(itb_ui_stash *stash);

//input

//input handling
//ie ITB_K_CTRL('a')
#define ITB_K_CTRL(c) ((c)&0x1f)

//start at the range outside char
#define ITB_K_LEFT    (1 << 9)
#define ITB_K_RIGHT   (2 << 9)
#define ITB_K_UP      (3 << 9)
#define ITB_K_DOWN    (4 << 9)
#define ITB_K_PAGE_UP   (5 << 9)
#define ITB_K_PAGE_DOWN (6 << 9)
#define ITB_K_HOME    (7 << 9)
#define ITB_K_END     (8 << 9)
#define ITB_K_DELETE  (9 << 9)

//this is whats usually used but it could be 0x8 or ITB_K_CTRL('h')
#define ITB_K_BACKSPACE (127)
#define ITB_K_IS_BACKSPACE(c) ((c) == 127 || c == 8 || c == ITB_K_CTRL('h'))

//get a character
ITBDEF int32_t itb_ui_char();

#endif //ITB_UI_H
#ifdef ITB_UI_IMPLEMENTATION
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#if ITB_UI_UNICODE
#include <wchar.h>
#endif
//==>ncurses like replacement<==

//==>initialization and cleanup
int itb_ui_start(itb_ui_context *restrict ctx) {
    //only run on terminals
    if (!isatty(STDIN_FILENO)) {
        return 1;
    }

    //store the original terminal settings
    if (tcgetattr(STDIN_FILENO, &ctx->original)) {
        return 2;
    }

#if ITB_UI_UNICODE
    //use the environments locale
    if (!setlocale(LC_ALL, "")) {
        return 3;
    }

    //set it to multibyte
    if (fwide(stdout, 1) <= 0) {
        return 4;
    }
#endif

    //in testing, unbuffered was far less performant than buffered
    //
    //real 0m2.281s unbuffered
    //real 0m0.437s buffered
    //
    //setvbuf(stdout, NULL, _IONBF, 0);

    struct termios raw;

    raw = ctx->original;

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
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0; // immediate - anything

    // put terminal in raw mode after flushing
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {
        return 5;
    }

    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w)) {
        return 6;
    }

    size_t *rp = (size_t *)&ctx->rows;
    size_t *cp = (size_t *)&ctx->cols;

    *rp = w.ws_row;
    *cp = w.ws_col;

    //the initialization is for laying out the memory as follows
    //[page 0 rows][page 1 rows][actual data [page 0 cols][page 1 cols]][render line][color sets]

    //compute the sizes
    const size_t cell_count  = ctx->rows * ctx->cols;
    const size_t data_size   = cell_count * sizeof(ITB_CHAR);
    const size_t render_size = (ctx->cols + 1) * sizeof(ITB_CHAR);
    const size_t color_size  = cell_count * sizeof(itb_color_mode);

    //all memory is in one block
    uint8_t *temp = malloc((data_size + color_size) * 2 + render_size + color_size);

    if (!temp) {
        return 7;
    }

    //compute the offsets
    uint8_t *data0_offset  = (temp);
    uint8_t *data1_offset  = (temp + data_size);
    uint8_t *color0_offset = (temp + data_size * 2);
    uint8_t *color1_offset = (temp + data_size * 2 + color_size);

    //fill out the structure
    ctx->buffer[0]       = (ITB_CHAR *)data0_offset;
    ctx->buffer[1]       = (ITB_CHAR *)data1_offset;
    ctx->color_buffer[0] = (itb_color_mode *)color0_offset;
    ctx->color_buffer[1] = (itb_color_mode *)color1_offset;

    itb_ui_clear(ctx);

    //clear everything and move to the top left
    ITB_FPRINTF(stdout, ITB_T("\x1b[2J\x1b[H"));

    ctx->cursor[0] = 1; //x
    ctx->cursor[1] = 1; //y

    ctx->current_color.flags = -1;

    ctx->cursor_visible = true;

    ctx->is_dirty = true;

    return 0;
}

int itb_ui_end(itb_ui_context *restrict ctx) {
    if (!ctx->cursor_visible) {
        itb_ui_show(ctx);
    }

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ctx->original)) {
        return 1;
    }

    //only one malloc w00t
    free(ctx->buffer[0]);

    ITB_FPRINTF(stdout, ITB_RESET);

    return 0;
}

void itb_ui_clear(itb_ui_context *ctx) {
    const size_t max = ctx->cols * ctx->rows;

    ctx->is_dirty    = true;
    ctx->dirty_start = 0;
    ctx->dirty_end   = max;

    ITB_MEMSET(ctx->buffer[0], ITB_T(' '), max);
    for (size_t i = 0; i < max; ++i) {
        ctx->color_buffer[0][i].flags = -1;
    }
}

//==>output
int itb_ui_printf(itb_ui_context *restrict ctx, size_t row, size_t col, const ITB_CHAR *fmt, ...) {
    //bounds check
    if (row && row <= ctx->rows && col && col <= ctx->cols) {
        va_list args;
        int ret;
        va_start(args, fmt);

        //+1 for NULL terminator
        const size_t maxlen = ctx->cols - col + 1;
        ITB_CHAR tbuf[maxlen];

        ret = ITB_SPRINTF(tbuf, maxlen, fmt, args);

        if (ret > 0) {
            if ((size_t)ret > maxlen) {
                ret = maxlen;
            }
            size_t idx;
            ITB_UI_RC_IDX(ctx, row, col, idx);

            //characters
            ITB_MEMCPY(ctx->buffer[0] + idx, tbuf, ret);
            //color
            for (size_t i = 0; i < (size_t)ret; ++i) {
                ctx->color_buffer[0][idx + i].flags = ctx->current_color.flags;
            }
            ITB_UI_UPDATE_DIRTY(ctx, idx, idx + ret);
        }
        va_end(args);
        return ret;
    } else {
        return -1;
    }
}

int itb_ui_strcpy(itb_ui_context *ctx, size_t row, size_t col, const ITB_CHAR *str, size_t len) {
    if (row && row <= ctx->rows && col && col <= ctx->cols) {
        size_t idx;
        ITB_UI_RC_IDX(ctx, row, col, idx);

        //dont over copy
        if (ctx->cols - col < len) {
            len = ctx->cols - col;
        }

        //memcpy not strcpy because we dont want nulls added
        //characters
        ITB_MEMCPY(ctx->buffer[0] + idx, str, len);
        //color
        for (size_t i = 0; i < len; ++i) {
            ctx->color_buffer[0][idx + i].flags = ctx->current_color.flags;
        }

        ITB_UI_UPDATE_DIRTY(ctx, idx, idx + len);
        return len;
    } else {
        return -1;
    }
}

//==>color functionality
void itb_ui_color(itb_ui_context *ctx, itb_color_mode *mode) {
    ctx->current_color.flags = mode ? mode->flags : -1;
}

//==>cursor functionality
void itb_ui_mv(itb_ui_context *restrict ctx, size_t row, size_t col) {
    //only update if we actually need to
    if (ctx->cursor[0] != row || ctx->cursor[1] != col) {
        if (row == 1 && col == 1) {
            ITB_FPRINTF(stdout, ITB_T("\x1b[H"));
        } else {
            ITB_FPRINTF(stdout, ITB_T("\x1b[%ld;%ldf"), row, col);
        }

        ctx->cursor[0] = row;
        ctx->cursor[1] = col;
    } //else no change
}

void itb_ui_hide(itb_ui_context *restrict ctx) {
    if (ctx->cursor_visible) {
        ITB_FPRINTF(stdout, ITB_T("\x1b[?25l"));
        ctx->cursor_visible = false;
    }
}

void itb_ui_show(itb_ui_context *restrict ctx) {
    if (!ctx->cursor_visible) {
        ITB_FPRINTF(stdout, ITB_T("\x1b[?25h"));
        ctx->cursor_visible = true;
    }
}

//==>file internals
static inline void __itb_change_color(itb_color_mode *mode) {
    //while this is a fixed output that could be turned into a table
    //...im not writing an 8*8+8+8 switch to handle these cases
    if (mode->flags == -1) {
        ITB_FPRINTF(stdout, ITB_RESET);
    } else if (mode->set.fg != -1 && mode->set.bg != -1) {
        ITB_FPRINTF(stdout, ITB_T("\x1b[3%hhd;4%hhdm"), mode->set.fg, mode->set.bg);
    } else if (mode->set.fg != -1) {
        ITB_FPRINTF(stdout, ITB_T("\x1b[3%hhdm"), mode->set.fg);
    } else if (mode->set.bg != -1) {
        ITB_FPRINTF(stdout, ITB_T("\x1b[4%hhdm"), mode->set.bg);
    }
}

//move and color print
static inline void __itb_mv_print(
    itb_ui_context *restrict ctx, const size_t idx, const size_t width) {
    //set color
    //TODO only change color on change
    __itb_change_color(ctx->color_buffer[0] + idx);
    //move
    size_t row, col;
    ITB_UI_IDX_RC(ctx, idx, row, col);
    itb_ui_mv(ctx, row, col);

    //print
#if ITB_UI_UNICODE
    fwprintf(stdout, L"%.*ls", width, ctx->buffer[0] + idx);
#else
    fwrite(ctx->buffer[0] + idx, 1, width, stdout);
#endif
    ITB_MEMCPY(ctx->buffer[1] + idx, ctx->buffer[0] + idx, width);
    memcpy(ctx->color_buffer[1] + idx, ctx->color_buffer[0] + idx, width);
}

//returns width of next printable chunk setting the starting point
static inline size_t __itb_find_bound(
    itb_ui_context *restrict ctx, const size_t src_idx, size_t *restrict dst_idx) {
    const size_t max = ctx->rows * ctx->cols;

    itb_color_mode mode;
    mode.flags = ctx->color_buffer[0][src_idx].flags;

    size_t start = src_idx;
    //remove unchanged starting characters and colors
    //color change always stops loop
    while (start < max && !ITB_STRCMP(ctx->buffer[0] + start, ctx->buffer[1] + start, 1)
        && mode.flags == ctx->color_buffer[0][start].flags)
        ++start; //loop

    //fist change
    *dst_idx = start;

    if (start == max) {
        return 0;
    }

    size_t end = start;
    //if the change was a color this will allow efficient bounding as further changese that keep
    //this color can be batched
    mode.flags = ctx->color_buffer[0][start].flags;

    //get total changes in a row
    //color change always stops loop
    while (end < max && ITB_STRCMP(ctx->buffer[0] + end, ctx->buffer[1] + end, 1)
        && mode.flags == ctx->color_buffer[0][end].flags)
        ++end; //loop

    //end of changes
    return end - start;
}

void itb_ui_flip(itb_ui_context *restrict ctx) {
    if (!ctx->is_dirty) {
        //no changes
        return;
    }
    //save cursor position
    size_t cursor[2];
    cursor[0] = ctx->cursor[0];
    cursor[1] = ctx->cursor[1];

    bool isvisibile = ctx->cursor_visible;
    if (isvisibile) {
        itb_ui_hide(ctx);
    }

    size_t width = 0;
    for (size_t idx = ctx->dirty_start; idx < ctx->dirty_end; idx += width + 1) {
        //set idx to the new dest
        width = __itb_find_bound(ctx, idx, &idx);
        if (width) {
            __itb_mv_print(ctx, idx, width);
        }
    }

    //see comment in start for why its flushed each flip
    fflush(stdout);

    //restore previous state
    itb_ui_mv(ctx, cursor[0], cursor[1]);

    if (isvisibile) {
        itb_ui_show(ctx);
    }

    ctx->is_dirty = false;
}

void itb_ui_box(itb_ui_context *restrict ctx, size_t row, size_t col, size_t width, size_t height) {
    //only render boxes that are fully visable
    if (!row || !col || row + height > ctx->rows || col + width > ctx->cols || width < 2
        || height < 2) {
        return;
    }

    //corners
    size_t idx, minidx, maxidx;
    ITB_UI_RC_IDX(ctx, row, col, idx);
    minidx              = idx;
    ctx->buffer[0][idx] = ITB_SEL('+', L'┌');

    ctx->color_buffer[0][idx].flags = ctx->current_color.flags;

    ITB_UI_RC_IDX(ctx, row + height - 1, col, idx);
    ctx->buffer[0][idx] = ITB_SEL('+', L'└');

    ctx->color_buffer[0][idx].flags = ctx->current_color.flags;

    ITB_UI_RC_IDX(ctx, row, col + width - 1, idx);
    ctx->buffer[0][idx] = ITB_SEL('+', L'┐');

    ctx->color_buffer[0][idx].flags = ctx->current_color.flags;

    ITB_UI_RC_IDX(ctx, row + height - 1, col + width - 1, idx);
    ctx->buffer[0][idx] = ITB_SEL('+', L'┘');

    ctx->color_buffer[0][idx].flags = ctx->current_color.flags;

    maxidx = idx;
    ITB_UI_UPDATE_DIRTY(ctx, minidx, maxidx);

    //top and bottom line
    for (size_t c = col + 1; c < ctx->cols && c < col + width - 1; ++c) {
        ITB_UI_RC_IDX(ctx, row, c, idx);
        ctx->buffer[0][idx]             = ITB_SEL('-', L'─');
        ctx->color_buffer[0][idx].flags = ctx->current_color.flags;

        ITB_UI_RC_IDX(ctx, row + height - 1, c, idx);
        ctx->buffer[0][idx]             = ITB_SEL('-', L'─');
        ctx->color_buffer[0][idx].flags = ctx->current_color.flags;
    }
    //left and right line
    for (size_t r = row + 1; r < ctx->rows && r < row + height - 1; ++r) {
        ITB_UI_RC_IDX(ctx, r, col, idx);
        ctx->buffer[0][idx]             = ITB_SEL('|', L'│');
        ctx->color_buffer[0][idx].flags = ctx->current_color.flags;
        ITB_UI_RC_IDX(ctx, r, col + width - 1, idx);
        ctx->buffer[0][idx]             = ITB_SEL('|', L'│');
        ctx->color_buffer[0][idx].flags = ctx->current_color.flags;
    }
}

//==>stash
int itb_ui_stash_init(const itb_ui_context *ctx, itb_ui_stash *stash) {
    const size_t cells = ctx->rows * ctx->cols;

    uint8_t *temp = malloc(cells * (sizeof(ITB_CHAR) + sizeof(itb_color_mode)));

    if (temp) {
        stash->buffer = (ITB_CHAR *)temp;
        stash->colors = (itb_color_mode *)(temp + cells * sizeof(ITB_CHAR));
        return 0;
    } else {
        return 1;
    }
}

void itb_ui_stash_copy(const itb_ui_context *ctx, itb_ui_stash *stash) {
    const size_t cells = ctx->rows * ctx->cols;
    ITB_MEMCPY(stash->buffer, ctx->buffer[0], cells);
    memcpy(stash->colors, ctx->color_buffer[0], cells);
}

void itb_ui_stash_paste(itb_ui_context *ctx, itb_ui_stash *stash) {
    const size_t cells = ctx->rows * ctx->cols;

    ctx->is_dirty    = true;
    ctx->dirty_start = 0;
    ctx->dirty_end   = cells;

    ITB_MEMCPY(ctx->buffer[0], stash->buffer, cells);
    memcpy(ctx->color_buffer[0], stash->colors, cells);
}

void itb_ui_stash_close(itb_ui_stash *stash) {
    if (stash->buffer) {
        free(stash->buffer);
        stash->buffer = NULL;
    }
}

//==>input<==

//inspired by https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html
int32_t itb_ui_char() {
    char c;
    switch (read(STDIN_FILENO, &c, 1)) {
        case -1:
            //error
            return -1;
        case 0:
            //no data
            return '\0';
        case 1:
            break;
    }
    if (c != '\x1b') {
        return c;
    }

    //escape code 27
    char escape_code[3];

    if (read(STDIN_FILENO, escape_code, 1) != 1) {
        return '\x1b';
    }

    if (read(STDIN_FILENO, escape_code + 1, 1) != 1) {
        return '\x1b';
    }

    if (escape_code[0] == '[') {
        //additional byte for escape
        if (escape_code[1] >= '0' && escape_code[1] <= '9') {
            if (read(STDIN_FILENO, escape_code + 2, 1) != 1) {
                return '\x1b';
            }
            if (escape_code[2] == '~') {
                switch (escape_code[1]) {
                    case '1':
                    case '7':
                        return ITB_K_HOME;
                    case '4':
                    case '8':
                        return ITB_K_END;
                    case '3':
                        return ITB_K_DELETE;
                    case '5':
                        return ITB_K_PAGE_UP;
                    case '6':
                        return ITB_K_PAGE_DOWN;
                }
            }
        } else {
            switch (escape_code[1]) {
                case 'A':
                    return ITB_K_UP;
                case 'B':
                    return ITB_K_DOWN;
                case 'C':
                    return ITB_K_RIGHT;
                case 'D':
                    return ITB_K_LEFT;
                case 'H':
                    return ITB_K_HOME;
                case 'F':
                    return ITB_K_END;
            }
        }
    } else if (escape_code[0] == 'O') {
        switch (escape_code[1]) {
            case 'H':
                return ITB_K_HOME;
            case 'F':
                return ITB_K_END;
        }
    }

    //TODO, i dont think this can actually happen as the only thing that
    //prints just escape would be ESC thus the next byte would be a '\0'
    return '\x1b';
}
#endif //ITB_UI_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
