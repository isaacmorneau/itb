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
#else
#define __ITB_TEXT(x) x
#define ITB_FPRINTF fprintf
#define ITB_SPRINTF vsnprintf
#define ITB_MEMCPY memcpy
#define ITB_MEMSET memset
#define ITB_CHAR char
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
        uint8_t fg;
        uint8_t bg;
    } set;
    uint16_t flags;
} itb_color_mode;

//get the correct index in the flat buffers such as double_buff and color_buff
#define ITB_UI_CTX_INDEX(ctx, row, col) (((row) * (ctx)->cols) + (col))

//organized in decending order of size to minimize padding
typedef struct itb_ui_context {
    //for returning to normal terminal settings after
    struct termios original;
    //also corresponds to bottom right position, (1,1) top left
    const size_t rows;
    const size_t cols;
    //current row, current col
    size_t cursor[2];
    //set by itb_ui_dirty_box
    ssize_t dirty_min_row;
    ssize_t dirty_min_col;
    ssize_t dirty_max_row;
    ssize_t dirty_max_col;

    //flattened grid of what color mode is set rn
    //[row * col]
    itb_color_mode *color_buff;
    //0 - delta buffer
    //1 - last flipped
    //[row * col]
    ITB_CHAR *double_buff[2];
    //a temp buff for printf
    ITB_CHAR *render_line;

    itb_color_mode current_color;
    bool cursor_visible;
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
//force rerender it all, slow and should be avoided
ITBDEF void itb_ui_flip_force(itb_ui_context *ui_ctx);

//move the cursor to the pos
ITBDEF void itb_ui_mv(itb_ui_context *ui_ctx, size_t row, size_t col);

//set the current drawing color
//pass NULL for mode to rest
ITBDEF void itb_ui_color(itb_ui_context *ui_ctx, itb_color_mode *mode);

//update the color of a given region to the current drawing color
ITBDEF void itb_ui_color_line(itb_ui_context *ui_ctx, size_t row, size_t col, size_t length);
ITBDEF void itb_ui_color_box(
    itb_ui_context *ui_ctx, size_t row, size_t col, size_t width, size_t height);

//hide the cursor
ITBDEF void itb_ui_hide(itb_ui_context *ui_ctx);
//show the cursor
ITBDEF void itb_ui_show(itb_ui_context *ui_ctx);

//starts at the top left
ITBDEF void itb_ui_box(itb_ui_context *ui_ctx, size_t row, size_t col, size_t width, size_t height);

//mark a box as needing a redraw
ITBDEF void itb_ui_dirty_box(
    itb_ui_context *ui_ctx, size_t minrow, size_t mincol, size_t maxrow, size_t maxcol);

//starts at the top left
ITBDEF void itb_ui_clear(itb_ui_context *ui_ctx);

//starts at current cursor
ITBDEF int itb_ui_printf(itb_ui_context *ui_ctx, const ITB_CHAR *fmt, ...);

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
    uint8_t *temp = calloc(data_size * 2 + render_size + color_size, 1);

    if (!temp) {
        return 7;
    }

    //compute the offsets
    uint8_t *data0_offset  = (temp);
    uint8_t *data1_offset  = (temp + data_size);
    uint8_t *render_offset = (temp + data_size * 2);
    uint8_t *color_offset  = (temp + data_size * 2 + render_size);

    //fill out the structure
    ui_ctx->double_buff[0] = (ITB_CHAR *)data0_offset;
    ui_ctx->double_buff[1] = (ITB_CHAR *)data1_offset;
    ui_ctx->render_line    = (ITB_CHAR *)render_offset;
    ui_ctx->color_buff     = (itb_color_mode *)color_offset;

    //set data to empty spaces
    ITB_MEMSET((ITB_CHAR *)data0_offset, ITB_T(' '), cell_count * 2);

    //clear everything and move to the top left
    ITB_FPRINTF(stdout, ITB_T("\x1b[2J\x1b[H"));

    fflush(stdout);

    ui_ctx->cursor[0] = 0; //x
    ui_ctx->cursor[1] = 0; //y

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

    //for (size_t r = 0; r < ui_ctx->rows; ++r) {
    //    free(ui_ctx->double_buff[0][r]);
    //}

    free(ui_ctx->double_buff[0]);

    return 0;
}

void itb_ui_dirty_box(
    itb_ui_context *restrict ui_ctx, size_t minrow, size_t mincol, size_t maxrow, size_t maxcol) {
    if (minrow > 0 && (ui_ctx->dirty_min_row == -1 || (size_t)ui_ctx->dirty_min_row > minrow)) {
        ui_ctx->dirty_min_row = minrow;
    }

    if (mincol > 0 && (ui_ctx->dirty_min_col == -1 || (size_t)ui_ctx->dirty_min_col > mincol)) {
        ui_ctx->dirty_min_col = mincol;
    }

    if (maxrow > 0 && (ui_ctx->dirty_max_row == -1 || (size_t)ui_ctx->dirty_max_row < maxrow)) {
        ui_ctx->dirty_max_row = maxrow;
    }

    if (maxcol > 0 && (ui_ctx->dirty_max_col == -1 || (size_t)ui_ctx->dirty_max_col < maxcol)) {
        ui_ctx->dirty_max_col = maxcol;
    }
}

static inline void __itb_color_print(itb_ui_context *restrict ui_ctx, itb_color_mode *mode) {
    if (ui_ctx->current_color.flags != mode->flags) {
        ui_ctx->current_color.flags = mode->flags;
        //while this is a fixed output that could be turned into a table
        //...im not writing an 8*8+8+8 switch to handle these cases
        if (!mode->flags) {
            ITB_FPRINTF(stdout, ITB_RESET);
        } else if (mode->set.fg && mode->set.bg) {
            ITB_FPRINTF(stdout, ITB_T("\x1b[3%hhu,4%hhum"), mode->set.fg, mode->set.bg);
        } else if (mode->set.fg) {
            ITB_FPRINTF(stdout, ITB_T("\x1b[3%hhum"), mode->set.fg);
        } else if (mode->set.bg) {
            ITB_FPRINTF(stdout, ITB_T("\x1b[4%hhum"), mode->set.bg);
        }
    }
}

void itb_ui_flip(itb_ui_context *restrict ui_ctx) {
    if (ui_ctx->dirty_min_row == -1 && ui_ctx->dirty_min_col == -1 && ui_ctx->dirty_max_row == -1
        && ui_ctx->dirty_max_col == -1) {
        //there were no changes
        return;
    } else {
        //ensure that if any rows were only partially set they are corrected
        if (ui_ctx->dirty_min_row < 0) {
            ui_ctx->dirty_min_row = 0;
        }
        if (ui_ctx->dirty_min_col < 0) {
            ui_ctx->dirty_min_col = 0;
        }
        if (ui_ctx->dirty_max_row < 0 || (size_t)ui_ctx->dirty_max_row > ui_ctx->rows) {
            ui_ctx->dirty_max_row = ui_ctx->rows;
        }
        if (ui_ctx->dirty_max_col < 0 || (size_t)ui_ctx->dirty_max_col > ui_ctx->cols) {
            ui_ctx->dirty_max_col = ui_ctx->cols;
        }
    }
    size_t cursor[2];

    bool isvisibile = ui_ctx->cursor_visible;
    cursor[0]       = ui_ctx->cursor[0];
    cursor[1]       = ui_ctx->cursor[1];

    //move top left
    itb_ui_mv(ui_ctx, 1, 1);

    if (isvisibile) {
        itb_ui_hide(ui_ctx);
    }

    for (size_t r = (size_t)ui_ctx->dirty_min_row; r < (size_t)ui_ctx->dirty_max_row; ++r) {
        size_t offset = 0;
        size_t width  = 0;

        for (size_t c = (size_t)ui_ctx->dirty_min_col; c < (size_t)ui_ctx->dirty_max_col; ++c) {
            const size_t idx = ITB_UI_CTX_INDEX(ui_ctx, r, c);
            if (ui_ctx->color_buff[idx].flags ||
#if ITB_UI_UNICODE
                wcsncmp(ui_ctx->double_buff[0] + idx, ui_ctx->double_buff[1] + idx, 1)
#else
                strncmp(ui_ctx->double_buff[0] + idx, ui_ctx->double_buff[1] + idx, 1)
#endif
            ) {
                //it is different

                //if its the start of the delta mark it
                if (!width) {
                    offset = idx;
                    itb_ui_mv(ui_ctx, r, c);
                }

                ++width;
            } else if (width) {
                //it was different

                //TODO test if its faster to instead of doing delta copies to just copy the whole thing in one go afterwards
                int ret;
                __itb_color_print(ui_ctx, ui_ctx->color_buff + offset);
#if ITB_UI_UNICODE
                ret = fwprintf(stdout, L"%.*ls", width, ui_ctx->double_buff[0] + offset);
#else
                ret = fwrite(ui_ctx->double_buff[0] + offset, 1, width, stdout);
#endif
                if (ret > 0) {
                    ITB_MEMCPY(
                        ui_ctx->double_buff[1] + offset, ui_ctx->double_buff[0] + offset, ret);
                }
                width = 0;
            }
        }
        //catch EOL deltas
        if (width) {
            //TODO test if its faster to instead of doing delta copies to just copy the whole thing in one go afterwards
            //while i could check the affected colmns in this case the double buffer is pre sanitized and i dont need to
#if ITB_UI_UNICODE
            fwprintf(stdout, L"%.*ls", width, ui_ctx->double_buff[0] + offset);
#else
            fwrite(ui_ctx->double_buff[0] + offset, 1, width, stdout);
#endif
            ITB_MEMCPY(ui_ctx->double_buff[1] + offset, ui_ctx->double_buff[0] + offset, width);
        }
    }

    //restore previous state
    itb_ui_mv(ui_ctx, cursor[0], cursor[1]);

    if (isvisibile) {
        itb_ui_show(ui_ctx);
    }

    fflush(stdout);

    //reset bounds
    ui_ctx->dirty_min_row = -1;
    ui_ctx->dirty_min_col = -1;
    ui_ctx->dirty_max_row = -1;
    ui_ctx->dirty_max_col = -1;
}

void itb_ui_flip_force(itb_ui_context *restrict ui_ctx) {
    size_t cursor[2];
    bool isvisibile = ui_ctx->cursor_visible;
    cursor[0]       = ui_ctx->cursor[0];
    cursor[1]       = ui_ctx->cursor[1];

    //move top left
    itb_ui_hide(ui_ctx);

    for (size_t r = 0; r < ui_ctx->rows; ++r) {
        itb_ui_mv(ui_ctx, r + 1, 1);
#if ITB_UI_UNICODE
        fwprintf(stdout, L"%.*ls", ui_ctx->cols,
            ui_ctx->double_buff[0] + ITB_UI_CTX_INDEX(ui_ctx, r, 0));
#else
        fwrite(ui_ctx->double_buff[0] + ITB_UI_CTX_INDEX(ui_ctx, r, 0), 1, ui_ctx->cols, stdout);
#endif
    }

    ITB_MEMCPY(ui_ctx->double_buff[1], ui_ctx->double_buff[0], ui_ctx->rows * ui_ctx->cols);

    //restore previous state
    itb_ui_mv(ui_ctx, cursor[0], cursor[1]);

    if (isvisibile) {
        itb_ui_show(ui_ctx);
    }
    fflush(stdout);
}

void itb_ui_color(itb_ui_context *restrict ui_ctx, itb_color_mode *mode) {
    ui_ctx->current_color.flags = mode ? mode->flags : 0;
}

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

void itb_ui_box(
    itb_ui_context *restrict ui_ctx, size_t row, size_t col, size_t width, size_t height) {
    //if this is outside we cant render any of it
    if (row > ui_ctx->rows || col > ui_ctx->cols || !row || !col || width < 2 || height < 2) {
        return;
    }

    //1 based to 0 based
    --row;
    --col;

    //tl
#if ITB_UI_UNICODE
    ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row, col)] = L'┌';
#else
    ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row, col)] = '+';
#endif

    itb_ui_dirty_box(ui_ctx, row, col, row + height, col + width);
    itb_ui_color_box(ui_ctx, row, col, width, height);

    if (row + height < ui_ctx->rows && col + width < ui_ctx->cols) {
        //bl
#if ITB_UI_UNICODE
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row + height - 1, col)] = L'└';
        //tr
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row, col + width - 1)] = L'┐';
        //br
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row + height - 1, col + width - 1)] = L'┘';
#else
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row + height - 1, col)] = '+';
        //tr
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row, col + width - 1)] = '+';
        //br
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row + height - 1, col + width - 1)] = '+';
#endif
    } else if (col + width < ui_ctx->cols) {
        // tr only
#if ITB_UI_UNICODE
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row, col + width - 1)] = L'┐';
#else
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row, col + width - 1)] = '+';
#endif
    } else if (row + height < ui_ctx->rows) {
        // bl only
#if ITB_UI_UNICODE
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row + height - 1, col)] = L'└';
#else
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row + height - 1, col)] = '+';
#endif
    }

    //top line can at least start
    for (size_t c = col + 1; c < ui_ctx->cols && c < col + width - 1; ++c) {
#if ITB_UI_UNICODE
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row, c)] = L'─';
#else
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row, c)] = '-';
#endif
    }

    //bottom line may be off screen
    if (row + height < ui_ctx->rows) {
        for (size_t c = col + 1; c < ui_ctx->cols && c < col + width - 1; ++c) {
#if ITB_UI_UNICODE
            ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row + height - 1, c)] = L'─';
#else
            ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, row + height - 1, c)] = '-';
#endif
        }
    }

    //left line can at least start
    for (size_t r = row + 1; r < ui_ctx->rows && r < row + height - 1; ++r) {
#if ITB_UI_UNICODE
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, r, col)] = L'│';
#else
        ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, r, col)] = '|';
#endif
    }

    //right line may be off screen
    if (col + width < ui_ctx->cols) { // tr only
        for (size_t r = row + 1; r < ui_ctx->rows && r < row + height - 1; ++r) {
#if ITB_UI_UNICODE
            ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, r, col + width - 1)] = L'│';
#else
            ui_ctx->double_buff[0][ITB_UI_CTX_INDEX(ui_ctx, r, col + width - 1)] = '|';
#endif
        }
    }
}

void itb_ui_clear(itb_ui_context *restrict ui_ctx) {
    ITB_MEMSET(ui_ctx->double_buff[0], ITB_T(' '), ui_ctx->cols * ui_ctx->rows);
    memset(ui_ctx->color_buff, 0, ui_ctx->cols * ui_ctx->rows * sizeof(itb_color_mode));
}

int itb_ui_printf(itb_ui_context *restrict ui_ctx, const ITB_CHAR *fmt, ...) {
    va_list args;
    int ret;
    va_start(args, fmt);
    ret = ITB_SPRINTF(ui_ctx->render_line, ui_ctx->cols - ui_ctx->cursor[1] + 1, fmt, args);

    if (ret > 0) {
        if ((size_t)ret > ui_ctx->cols - ui_ctx->cursor[1] + 1) {
            ret = ui_ctx->cols - ui_ctx->cursor[1] + 1;
        }
        itb_ui_dirty_box(ui_ctx, ui_ctx->cursor[0], ui_ctx->cursor[1], ui_ctx->cursor[0],
            ui_ctx->cursor[1] + ret);

        ITB_MEMCPY(
            ui_ctx->double_buff[0] + ITB_UI_CTX_INDEX(ui_ctx, ui_ctx->cursor[0], ui_ctx->cursor[1]),
            ui_ctx->render_line, ret);
        itb_ui_color_line(ui_ctx, ui_ctx->cursor[0], ui_ctx->cursor[1], ret);
    }
    va_end(args);
    return ret;
}

int itb_ui_rcprintf(
    itb_ui_context *restrict ui_ctx, size_t row, size_t col, const ITB_CHAR *fmt, ...) {
    if (row && row <= ui_ctx->rows && col && col <= ui_ctx->cols) {
        va_list args;
        int ret;
        va_start(args, fmt);
        ret = ITB_SPRINTF(ui_ctx->render_line, ui_ctx->cols - col + 1, fmt, args);

        if (ret > 0) {
            if ((size_t)ret > ui_ctx->cols - col + 1) {
                ret = ui_ctx->cols - col + 1;
            }

            itb_ui_dirty_box(ui_ctx, row, col, row, col + ret);

            ITB_MEMCPY(ui_ctx->double_buff[0] + ITB_UI_CTX_INDEX(ui_ctx, row, col),
                ui_ctx->render_line, ret);

            itb_ui_color_line(ui_ctx, row, col, ret);
        }
        va_end(args);
        return ret;
    } else {
        return -1;
    }
}

void itb_ui_color_line(itb_ui_context *restrict ui_ctx, size_t row, size_t col, size_t length) {
    //1 to 0 index and bouds check
    if (row-- == 0 || col-- == 0 || row >= ui_ctx->rows || col >= ui_ctx->cols) {
        //out of bounds
        return;
    }

    //set the start of the color
    ui_ctx->color_buff[ITB_UI_CTX_INDEX(ui_ctx, row, col)] = ui_ctx->current_color;
    //clear out any other existing colors
    if (length > 1) {
        //trim length
        if (col + length > ui_ctx->cols) {
            length = ui_ctx->cols - col;
        }
        memset(ui_ctx->color_buff + ITB_UI_CTX_INDEX(ui_ctx, row, col), 0, length);
    }
}

void itb_ui_color_box(
    itb_ui_context *restrict ui_ctx, size_t row, size_t col, size_t width, size_t height) {
    for (size_t h = row; h < row + height; ++h) {
        itb_ui_color_line(ui_ctx, h, col, width);
    }
}

#endif //ITB_UI_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
