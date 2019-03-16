/*
 * origin: https://github.com/isaacmorneau/itb
 *
 * These wrappers are shared across multiple projects and are collected here
 * to make it easier to add to new projects and backport fixes
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
    //a temp buff for printf
    ITB_CHAR *render_line;

    //current painting color
    itb_color_mode current_color;
    //last painted color
    //itb_color_mode last_color;
    bool cursor_visible;
    //paint needed
    bool is_dirty;
} itb_ui_context;

//functions as init and close but also sets the terminal modes to raw and back
//must be called first
//this should be set before any printing happens in the program
ITBDEF int itb_ui_start(itb_ui_context *ui_ctx);
//must be called last
ITBDEF int itb_ui_end(itb_ui_context *ui_ctx);

//render the scene, fast and prefered
//if manual updates were applied to the buffers make sure to mark them with itb_dirty_box
ITBDEF void itb_ui_flip(itb_ui_context *ui_ctx);

//move the cursor
ITBDEF void itb_ui_mv(itb_ui_context *ui_ctx, size_t row, size_t col);
//hide the cursor
ITBDEF void itb_ui_hide(itb_ui_context *ui_ctx);
//show the cursor
ITBDEF void itb_ui_show(itb_ui_context *ui_ctx);

//set the current drawing color
//pass NULL for mode to rest
ITBDEF void itb_ui_color(itb_ui_context *ui_ctx, itb_color_mode *mode);

//update the color of a given region to the current drawing color
ITBDEF void itb_ui_color_line(itb_ui_context *ui_ctx, size_t row, size_t col, size_t length);
ITBDEF void itb_ui_color_box(
    itb_ui_context *ui_ctx, size_t row, size_t col, size_t width, size_t height);

//starts at the top left
ITBDEF void itb_ui_box(itb_ui_context *ui_ctx, size_t row, size_t col, size_t width, size_t height);

//mark a box as needing a redraw
ITBDEF void itb_ui_dirty_box(
    itb_ui_context *ui_ctx, size_t row, size_t col, size_t width, size_t height);

//starts at the top left
ITBDEF void itb_ui_clear(itb_ui_context *ui_ctx);

//starts at row and col specified
ITBDEF int itb_ui_rcprintf(
    itb_ui_context *ui_ctx, size_t row, size_t col, const ITB_CHAR *fmt, ...);

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
int itb_ui_start(itb_ui_context *restrict ui_ctx) {
    //only run on terminals
    if (!isatty(STDIN_FILENO)) {
        return 1;
    }

    //store the original terminal settings
    if (tcgetattr(STDIN_FILENO, &ui_ctx->original)) {
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

    size_t *rp = (size_t *)&ui_ctx->rows;
    size_t *cp = (size_t *)&ui_ctx->cols;

    *rp = w.ws_row;
    *cp = w.ws_col;

    //the initialization is for laying out the memory as follows
    //[page 0 rows][page 1 rows][actual data [page 0 cols][page 1 cols]][render line][color sets]

    //compute the sizes
    const size_t cell_count  = ui_ctx->rows * ui_ctx->cols;
    const size_t data_size   = cell_count * sizeof(ITB_CHAR);
    const size_t render_size = (ui_ctx->cols + 1) * sizeof(ITB_CHAR);
    const size_t color_size  = cell_count * sizeof(itb_color_mode);

    //all memory is in one block
    uint8_t *temp = malloc((data_size + color_size) * 2 + render_size + color_size);

    if (!temp) {
        return 7;
    }

    //compute the offsets
    uint8_t *data0_offset  = (temp);
    uint8_t *data1_offset  = (temp + data_size);
    uint8_t *render_offset = (temp + data_size * 2);
    uint8_t *color0_offset = (temp + data_size * 2 + render_size);
    uint8_t *color1_offset = (temp + data_size * 2 + render_size + color_size);

    //fill out the structure
    ui_ctx->buffer[0]       = (ITB_CHAR *)data0_offset;
    ui_ctx->buffer[1]       = (ITB_CHAR *)data1_offset;
    ui_ctx->render_line     = (ITB_CHAR *)render_offset;
    ui_ctx->color_buffer[0] = (itb_color_mode *)color0_offset;
    ui_ctx->color_buffer[1] = (itb_color_mode *)color1_offset;

    itb_ui_clear(ui_ctx);

    //clear everything and move to the top left
    ITB_FPRINTF(stdout, ITB_T("\x1b[2J\x1b[H"));

    ui_ctx->cursor[0] = 1; //x
    ui_ctx->cursor[1] = 1; //y

    fflush(stdout);

    ui_ctx->cursor_visible = true;

    return 0;
}

int itb_ui_end(itb_ui_context *restrict ui_ctx) {
    if (!ui_ctx->cursor_visible) {
        itb_ui_show(ui_ctx);
    }

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ui_ctx->original)) {
        return 1;
    }

    //only one malloc w00t
    free(ui_ctx->buffer[0]);

    ITB_FPRINTF(stdout, ITB_RESET);

    return 0;
}

void itb_ui_clear(itb_ui_context *ui_ctx) {
    const size_t max = ui_ctx->cols * ui_ctx->rows;

    ui_ctx->is_dirty    = true;
    ui_ctx->dirty_start = 0;
    ui_ctx->dirty_end   = max;

    ITB_MEMSET(ui_ctx->buffer[0], ITB_T(' '), max);
    for (size_t i = 0; i < max; ++i) {
        ui_ctx->color_buffer[0][i].flags = -1;
    }
}

//==>formatted output
int itb_ui_rcprintf(
    itb_ui_context *restrict ui_ctx, size_t row, size_t col, const ITB_CHAR *fmt, ...) {
    //bounds check
    if (row && row <= ui_ctx->rows && col && col <= ui_ctx->cols) {
        va_list args;
        int ret;
        va_start(args, fmt);

        //+1 for NULL terminator
        const size_t maxlen = ui_ctx->cols - col + 1;

        ret = ITB_SPRINTF(ui_ctx->render_line, maxlen, fmt, args);

        if (ret > 0) {
            if ((size_t)ret > maxlen) {
                ret = maxlen;
            }
            size_t idx;
            ITB_UI_RC_IDX(ui_ctx, row, col, idx);

            ITB_MEMCPY(ui_ctx->buffer[0] + idx, ui_ctx->render_line, ret);
            for (size_t i = 0; i < (size_t)ret; ++i) {
                ui_ctx->color_buffer[0][idx + i].flags = ui_ctx->current_color.flags;
            }
            ITB_UI_UPDATE_DIRTY(ui_ctx, idx, idx + ret);
        }
        va_end(args);
        return ret;
    } else {
        return -1;
    }
}

//==>color functionality
void itb_ui_color(itb_ui_context *ui_ctx, itb_color_mode *mode) {
    ui_ctx->current_color.flags = mode ? mode->flags : -1;
}

//==>cursor functionality
void itb_ui_mv(itb_ui_context *restrict ui_ctx, size_t row, size_t col) {
    //only update if we actually need to
    if (ui_ctx->cursor[0] != row || ui_ctx->cursor[1] != col) {
        if (row == 1 && col == 1) {
            ITB_FPRINTF(stdout, ITB_T("\x1b[H"));
        } else {
            ITB_FPRINTF(stdout, ITB_T("\x1b[%ld;%ldf"), row, col);
        }

        //this feels overkill
        fflush(stdout);

        ui_ctx->cursor[0] = row;
        ui_ctx->cursor[1] = col;
    } //else no change
}

void itb_ui_hide(itb_ui_context *restrict ui_ctx) {
    if (ui_ctx->cursor_visible) {
        ITB_FPRINTF(stdout, ITB_T("\x1b[?25l"));
        ui_ctx->cursor_visible = false;
    }
}

void itb_ui_show(itb_ui_context *restrict ui_ctx) {
    if (!ui_ctx->cursor_visible) {
        ITB_FPRINTF(stdout, ITB_T("\x1b[?25h"));
        ui_ctx->cursor_visible = true;
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
    itb_ui_context *restrict ui_ctx, const size_t idx, const size_t width) {
    //set color
    //TODO only change color on change
    __itb_change_color(ui_ctx->color_buffer[0] + idx);
    //move
    size_t row, col;
    ITB_UI_IDX_RC(ui_ctx, idx, row, col);
    itb_ui_mv(ui_ctx, row, col);

    //print
#if ITB_UI_UNICODE
    fwprintf(stdout, L"%.*ls", width, ui_ctx->buffer[0] + idx);
#else
    fwrite(ui_ctx->buffer[0] + idx, 1, width, stdout);
#endif
    //TODO test if its faster to instead of doing delta copies to just copy the whole thing in one go afterwards
    ITB_MEMCPY(ui_ctx->buffer[1] + idx, ui_ctx->buffer[0] + idx, width);
    memcpy(ui_ctx->color_buffer[1] + idx, ui_ctx->color_buffer[0] + idx, width);
}

//returns width of next printable chunk setting the starting point
static inline size_t __itb_find_bound(
    itb_ui_context *restrict ui_ctx, const size_t src_idx, size_t *restrict dst_idx) {
    const size_t max = ui_ctx->rows * ui_ctx->cols;

    itb_color_mode mode;
    mode.flags = ui_ctx->color_buffer[0][src_idx].flags;

    size_t start = src_idx;
    //remove unchanged starting characters and colors
    //color change always stops loop
    while (start < max && !ITB_STRCMP(ui_ctx->buffer[0] + start, ui_ctx->buffer[1] + start, 1)
        && mode.flags == ui_ctx->color_buffer[0][start].flags)
        ++start; //loop

    //fist change
    *dst_idx = start;

    if (start == max) {
        return 0;
    }

    size_t end = start;
    //if the change was a color this will allow efficient bounding as further changese that keep
    //this color can be batched
    mode.flags = ui_ctx->color_buffer[0][start].flags;

    //get total changes in a row
    //color change always stops loop
    while (end < max && ITB_STRCMP(ui_ctx->buffer[0] + end, ui_ctx->buffer[1] + end, 1)
        && mode.flags == ui_ctx->color_buffer[0][end].flags)
        ++end; //loop

    //end of changes
    return end - start;
}

void itb_ui_flip(itb_ui_context *restrict ui_ctx) {
    if (!ui_ctx->is_dirty) {
        //no changes
        return;
    }
    //save cursor position
    size_t cursor[2];
    cursor[0] = ui_ctx->cursor[0];
    cursor[1] = ui_ctx->cursor[1];

    bool isvisibile = ui_ctx->cursor_visible;
    if (isvisibile) {
        itb_ui_hide(ui_ctx);
    }

    size_t width = 0;
    for (size_t idx = ui_ctx->dirty_start; idx < ui_ctx->dirty_end; idx += width + 1) {
        //set idx to the new dest
        width = __itb_find_bound(ui_ctx, idx, &idx);
        if (width) {
            __itb_mv_print(ui_ctx, idx, width);
        }
    }

    //restore previous state
    itb_ui_mv(ui_ctx, cursor[0], cursor[1]);

    if (isvisibile) {
        itb_ui_show(ui_ctx);
    }

    fflush(stdout);
    ui_ctx->is_dirty = false;
}

void itb_ui_box(
    itb_ui_context *restrict ui_ctx, size_t row, size_t col, size_t width, size_t height) {
    //only render boxes that are fully visable
    if (!row || !col || row + height > ui_ctx->rows || col + width > ui_ctx->cols || width < 2
        || height < 2) {
        return;
    }

    //corners
    size_t idx, minidx, maxidx;
    ITB_UI_RC_IDX(ui_ctx, row, col, idx);
    minidx                 = idx;
    ui_ctx->buffer[0][idx] = ITB_SEL('+', L'┌');

    ui_ctx->color_buffer[0][idx].flags = ui_ctx->current_color.flags;

    ITB_UI_RC_IDX(ui_ctx, row + height - 1, col, idx);
    ui_ctx->buffer[0][idx] = ITB_SEL('+', L'└');

    ui_ctx->color_buffer[0][idx].flags = ui_ctx->current_color.flags;

    ITB_UI_RC_IDX(ui_ctx, row, col + width - 1, idx);
    ui_ctx->buffer[0][idx] = ITB_SEL('+', L'┐');

    ui_ctx->color_buffer[0][idx].flags = ui_ctx->current_color.flags;

    ITB_UI_RC_IDX(ui_ctx, row + height - 1, col + width - 1, idx);
    ui_ctx->buffer[0][idx] = ITB_SEL('+', L'┘');

    ui_ctx->color_buffer[0][idx].flags = ui_ctx->current_color.flags;

    maxidx = idx;
    ITB_UI_UPDATE_DIRTY(ui_ctx, minidx, maxidx);

    //top and bottom line
    for (size_t c = col + 1; c < ui_ctx->cols && c < col + width - 1; ++c) {
        ITB_UI_RC_IDX(ui_ctx, row, c, idx);
        ui_ctx->buffer[0][idx]             = ITB_SEL('-', L'─');
        ui_ctx->color_buffer[0][idx].flags = ui_ctx->current_color.flags;

        ITB_UI_RC_IDX(ui_ctx, row + height - 1, c, idx);
        ui_ctx->buffer[0][idx]             = ITB_SEL('-', L'─');
        ui_ctx->color_buffer[0][idx].flags = ui_ctx->current_color.flags;
    }
    //left and right line
    for (size_t r = row + 1; r < ui_ctx->rows && r < row + height - 1; ++r) {
        ITB_UI_RC_IDX(ui_ctx, r, col, idx);
        ui_ctx->buffer[0][idx]             = ITB_SEL('|', L'│');
        ui_ctx->color_buffer[0][idx].flags = ui_ctx->current_color.flags;
        ITB_UI_RC_IDX(ui_ctx, r, col + width - 1, idx);
        ui_ctx->buffer[0][idx]             = ITB_SEL('|', L'│');
        ui_ctx->color_buffer[0][idx].flags = ui_ctx->current_color.flags;
    }
}

#endif //ITB_UI_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
