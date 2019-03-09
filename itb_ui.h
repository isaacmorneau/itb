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
#if ITB_UI_UNICODE
#include <wchar.h>
#define __ITB_TEXT(x) L##x
#else
#define __ITB_TEXT(x) x
#endif

#define __ITB_COLOR_CONV(x) #x
#define ITB_COLOR(fg, bg) __ITB_TEXT("\x1b[3" __ITB_COLOR_CONV(fg) "m;4" __ITB_COLOR_CONV(bg) "m")

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

typedef struct itb_ui_context {
    //for returning to normal terminal settings after
    struct termios original;
    //also corresponds to bottom right position, (1,1) top left
    size_t rows;
    size_t cols;
    //current row, current col
    size_t cursor[2];
    //min row, min col, max row, max col
    //set by itb_ui_dirty_box
    ssize_t dirty[4];
    //x*y*2
    size_t buffsize;
    //0 - delta buffer
    //1 - last flipped
#if ITB_UI_UNICODE
    wchar_t **double_buff[2];
    //a temp buff for printf
    wchar_t *render_line;
#else
    char **double_buff[2];
    //a temp buff for printf
    char *render_line;
#endif
    bool cursor_visible;
} itb_ui_context;

//functions as init and close but also sets the terminal modes to raw and back
//
//must be called first
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
#if ITB_UI_UNICODE
ITBDEF int itb_ui_printf(itb_ui_context *ui_ctx, const wchar_t *fmt, ...);
#else
ITBDEF int itb_ui_printf(itb_ui_context *ui_ctx, const char *fmt, ...);
#endif

//starts at row and col specified
#if ITB_UI_UNICODE
ITBDEF int itb_ui_rcprintf(itb_ui_context *ui_ctx, size_t row, size_t col, const wchar_t *fmt, ...);
#else
ITBDEF int itb_ui_rcprintf(itb_ui_context *ui_ctx, size_t row, size_t col, const char *fmt, ...);
#endif

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

int itb_ui_start(itb_ui_context *ui_ctx) {
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

    ui_ctx->rows = w.ws_row;
    ui_ctx->cols = w.ws_col;

    //the initialization is for laying out the memory as follows
    //[page 0 rows][page 1 rows][page 0 cols][page 1 cols]

    size_t row_size;
    size_t col_size;
    size_t data_size;
    size_t line_size;

#if ITB_UI_UNICODE
    row_size  = ui_ctx->rows * sizeof(wchar_t **);
    col_size  = ui_ctx->cols * sizeof(wchar_t);
    data_size = ui_ctx->rows * ui_ctx->cols * sizeof(wchar_t);
    line_size = (ui_ctx->cols + 1) * sizeof(wchar_t);
#else
    row_size = ui_ctx->rows * sizeof(char **);
    col_size = ui_ctx->cols * sizeof(char);
    data_size = ui_ctx->rows * ui_ctx->cols * sizeof(char);
    line_size = (ui_ctx->cols + 1) * sizeof(char);
#endif

    uint8_t *temp = malloc((row_size + data_size) * 2 + line_size);

    if (!temp) {
        return 7;
    }

    uint8_t *row1_offset   = (temp);
    uint8_t *row2_offset   = (temp + row_size);
    uint8_t *data1_offset  = (temp + row_size * 2);
    uint8_t *data2_offset  = (temp + row_size * 2 + data_size);
    uint8_t *render_offset = (temp + (row_size + data_size) * 2);

#if ITB_UI_UNICODE
    ui_ctx->double_buff[0] = (wchar_t **)row1_offset;
    ui_ctx->double_buff[1] = (wchar_t **)row2_offset;
    ui_ctx->render_line    = (wchar_t *)render_offset;
#else
    ui_ctx->double_buff[0] = (char **)row1_offset;
    ui_ctx->double_buff[1] = (char **)row2_offset;
    ui_ctx->render_line = (char *)render_offset;
#endif

    for (size_t r = 0; r < ui_ctx->rows; ++r) {
        //ui_ctx->double_buff[0][r] = malloc(ui_ctx->cols * 2);
#if ITB_UI_UNICODE
        ui_ctx->double_buff[0][r] = (wchar_t *)(data1_offset + (r * col_size));
        ui_ctx->double_buff[1][r] = (wchar_t *)(data2_offset + (r * col_size));
#else
        ui_ctx->double_buff[0][r] = (char *)(data1_offset + (r * col_size));
        ui_ctx->double_buff[1][r] = (char *)(data2_offset + (r * col_size));
#endif
    }

#if ITB_UI_UNICODE
    //you may think? "oh i know ill use the datasize"
    //no wmemset takes character counts not data size
    wmemset((wchar_t *)data1_offset, L' ', ui_ctx->rows * ui_ctx->cols * 2);
#else
    memset(data1_offset, ' ', ui_ctx->rows * ui_ctx->cols * 2);
#endif

    //clear everything and move to the top left
#if ITB_UI_UNICODE
    fwprintf(stdout, L"\x1b[2J\x1b[H");
#else
    fputs("\x1b[2J\x1b[H", stdout);
#endif

    fflush(stdout);

    ui_ctx->cursor[0] = 0; //x
    ui_ctx->cursor[1] = 0; //y

    ui_ctx->cursor_visible = true;

    return 0;
}

int itb_ui_end(itb_ui_context *ui_ctx) {
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
    itb_ui_context *ui_ctx, size_t minrow, size_t mincol, size_t maxrow, size_t maxcol) {
    if (minrow > 0 && (ui_ctx->dirty[0] == -1 || (size_t)ui_ctx->dirty[0] > minrow)) {
        ui_ctx->dirty[0] = minrow;
    }

    if (mincol > 0 && (ui_ctx->dirty[1] == -1 || (size_t)ui_ctx->dirty[1] > mincol)) {
        ui_ctx->dirty[1] = mincol;
    }

    if (maxrow > 0 && (ui_ctx->dirty[2] == -1 || (size_t)ui_ctx->dirty[2] < maxrow)) {
        ui_ctx->dirty[2] = maxrow;
    }

    if (maxcol > 0 && (ui_ctx->dirty[3] == -1 || (size_t)ui_ctx->dirty[3] < maxcol)) {
        ui_ctx->dirty[3] = maxcol;
    }
}

void itb_ui_flip(itb_ui_context *ui_ctx) {
    if (ui_ctx->dirty[0] == -1 && ui_ctx->dirty[1] == -1) {
        //there were no changes
        return;
    } else {
        //ensure that if any rows were only partially set they are corrected
        if (ui_ctx->dirty[0] < 0)
            ui_ctx->dirty[0] = 0;
        if (ui_ctx->dirty[1] < 0)
            ui_ctx->dirty[1] = 0;
        if (ui_ctx->dirty[2] < 0 || (size_t)ui_ctx->dirty[2] > ui_ctx->rows)
            ui_ctx->dirty[2] = ui_ctx->rows;
        if (ui_ctx->dirty[3] < 0 || (size_t)ui_ctx->dirty[3] > ui_ctx->cols)
            ui_ctx->dirty[3] = ui_ctx->cols;
    }
    //TODO record the last even so updates dont happen at all when nothings changed
    size_t cursor[2];

    bool isvisibile = ui_ctx->cursor_visible;
    cursor[0]       = ui_ctx->cursor[0];
    cursor[1]       = ui_ctx->cursor[1];

    //move top left
    itb_ui_mv(ui_ctx, 1, 1);

    if (isvisibile) {
        itb_ui_hide(ui_ctx);
    }

    for (size_t r = (size_t)ui_ctx->dirty[0]; r < (size_t)ui_ctx->dirty[2]; ++r) {
        size_t col   = 0;
        size_t width = 0;

        for (size_t c = (size_t)ui_ctx->dirty[1]; c < (size_t)ui_ctx->dirty[3]; ++c) {
#if ITB_UI_UNICODE
            if (wcsncmp(ui_ctx->double_buff[0][r] + c, ui_ctx->double_buff[1][r] + c, 1)) {
#else
            if (strncmp(ui_ctx->double_buff[0][r] + c, ui_ctx->double_buff[1][r] + c, 1)) {
#endif
                //it is different

                //if its the start of the delta mark it
                if (!width) {
                    col = c;
                }

                ++width;
            } else if (width) {
                //it was different

                itb_ui_mv(ui_ctx, r, col);

                int ret;

                //TODO test if its faster to instead of doing delta copies to just copy the whole thing in one go afterwards
#if ITB_UI_UNICODE
                ret = fwprintf(stdout, L"%.*ls", width, ui_ctx->double_buff[0][r] + col);
                if (ret > 0) {
                    wmemcpy(ui_ctx->double_buff[1][r] + col, ui_ctx->double_buff[0][r] + col, ret);
                }
#else
                ret = fwrite(ui_ctx->double_buff[0][r] + col, 1, width, stdout);
                if (ret > 0) {
                    memcpy(ui_ctx->double_buff[1][r] + col, ui_ctx->double_buff[0][r] + col, ret);
                }
#endif
                width = 0;
            }
        }
        //catch EOL deltas
        if (width) {
            itb_ui_mv(ui_ctx, r, col);

            //TODO test if its faster to instead of doing delta copies to just copy the whole thing in one go afterwards

            //while i could check the affected colmns in this case the double buffer is pre sanitized and i dont need to
#if ITB_UI_UNICODE
            fwprintf(stdout, L"%.*ls", width, ui_ctx->double_buff[0][r] + col);
            wmemcpy(ui_ctx->double_buff[1][r] + col, ui_ctx->double_buff[0][r] + col, width);
#else
            fwrite(ui_ctx->double_buff[0][r] + col, 1, width, stdout);
            memcpy(ui_ctx->double_buff[1][r] + col, ui_ctx->double_buff[0][r] + col, width);
#endif
        }
    }

    //restore previous state
    itb_ui_mv(ui_ctx, cursor[0], cursor[1]);

    if (isvisibile) {
        itb_ui_show(ui_ctx);
    }

    fflush(stdout);

    //reset bounds
    ui_ctx->dirty[0] = 0;
    ui_ctx->dirty[1] = 0;
    ui_ctx->dirty[2] = 0;
    ui_ctx->dirty[3] = 0;
}

void itb_ui_flip_force(itb_ui_context *ui_ctx) {
    size_t cursor[2];
    bool isvisibile = ui_ctx->cursor_visible;
    cursor[0]       = ui_ctx->cursor[0];
    cursor[1]       = ui_ctx->cursor[1];

    //move top left
    itb_ui_hide(ui_ctx);

    for (size_t r = 0; r < ui_ctx->rows; ++r) {
        itb_ui_mv(ui_ctx, r + 1, 1);
#if ITB_UI_UNICODE
        fwprintf(stdout, L"%.*ls", ui_ctx->cols, ui_ctx->double_buff[0][r]);
#else
        fwrite(ui_ctx->double_buff[0][r], 1, ui_ctx->cols, stdout);
#endif
    }

#if ITB_UI_UNICODE
    wmemcpy(ui_ctx->double_buff[1][0], ui_ctx->double_buff[0][0], ui_ctx->rows * ui_ctx->cols);
#else
    memcpy(ui_ctx->double_buff[1][0], ui_ctx->double_buff[0][0], ui_ctx->rows * ui_ctx->cols);
#endif

    //restore previous state
    itb_ui_mv(ui_ctx, cursor[0], cursor[1]);

    if (isvisibile) {
        itb_ui_show(ui_ctx);
    }
    fflush(stdout);
}

void itb_ui_mv(itb_ui_context *ui_ctx, size_t row, size_t col) {
    //only update if we actually need to
    if (ui_ctx->cursor[0] != row || ui_ctx->cursor[1] != col) {
        if (row == 1 && col == 1) {
#if ITB_UI_UNICODE
            fwprintf(stdout, L"\x1b[H");
#else
            fputs("\x1b[H", stdout);
#endif
        } else {
#if ITB_UI_UNICODE
            fwprintf(stdout, L"\x1b[%ld;%ldf", row, col);
#else
            fprintf(stdout, "\x1b[%ld;%ldf", row, col);
#endif
        }

        //this feels overkill
        fflush(stdout);

        ui_ctx->cursor[0] = row;
        ui_ctx->cursor[1] = col;
    } //else no change
}

void itb_ui_hide(itb_ui_context *ui_ctx) {
    if (ui_ctx->cursor_visible) {
        fputs("\x1b[?25l", stdout);
        ui_ctx->cursor_visible = false;
    }
}

void itb_ui_show(itb_ui_context *ui_ctx) {
    if (!ui_ctx->cursor_visible) {
        fputs("\x1b[?25h", stdout);
        ui_ctx->cursor_visible = true;
    }
}

void itb_ui_box(itb_ui_context *ui_ctx, size_t row, size_t col, size_t width, size_t height) {
    //if this is outside we cant render any of it
    if (row > ui_ctx->rows || col > ui_ctx->cols || !row || !col || width < 2 || height < 2) {
        return;
    }

    //1 based to 0 based
    --row;
    --col;

#if ITB_UI_UNICODE
    wchar_t **buffer = ui_ctx->double_buff[0];
#else
    char **buffer = ui_ctx->double_buff[0];
#endif

    //tl
#if ITB_UI_UNICODE
    buffer[row][col] = L'┌';
#else
    buffer[row][col] = '+';
#endif

    itb_ui_dirty_box(ui_ctx, row, col, row + height, col + width);

    if (row + height < ui_ctx->rows && col + width < ui_ctx->cols) {
        //bl
#if ITB_UI_UNICODE
        buffer[row + height - 1][col] = L'└';
        //tr
        buffer[row][col + width - 1] = L'┐';
        //br
        buffer[row + height - 1][col + width - 1] = L'┘';
#else
        buffer[row + height - 1][col] = '+';
        //tr
        buffer[row][col + width - 1] = '+';
        //br
        buffer[row + height - 1][col + width - 1] = '+';
#endif
    } else if (col + width < ui_ctx->cols) {
        // tr only
#if ITB_UI_UNICODE
        buffer[row][col + width - 1] = L'┐';
#else
        buffer[row][col + width - 1] = '+';
#endif
    } else if (row + height < ui_ctx->rows) {
        // bl only
#if ITB_UI_UNICODE
        buffer[row + height - 1][col] = L'└';
#else
        buffer[row + height - 1][col] = '+';
#endif
    }

    //top line can at least start
    for (size_t c = col + 1; c < ui_ctx->cols && c < col + width - 1; ++c) {
#if ITB_UI_UNICODE
        buffer[row][c] = L'─';
#else
        buffer[row][c] = '-';
#endif
    }

    //bottom line may be off screen
    if (row + height < ui_ctx->rows) {
        for (size_t c = col + 1; c < ui_ctx->cols && c < col + width - 1; ++c) {
#if ITB_UI_UNICODE
            buffer[row + height - 1][c] = L'─';
#else
            buffer[row + height - 1][c] = '-';
#endif
        }
    }

    //left line can at least start
    for (size_t r = row + 1; r < ui_ctx->rows && r < row + height - 1; ++r) {
#if ITB_UI_UNICODE
        buffer[r][col] = L'│';
#else
        buffer[r][col] = '|';
#endif
    }

    //right line may be off screen
    if (col + width < ui_ctx->cols) { // tr only
        for (size_t r = row + 1; r < ui_ctx->rows && r < row + height - 1; ++r) {
#if ITB_UI_UNICODE
            buffer[r][col + width - 1] = L'│';
#else
            buffer[r][col + width - 1] = '|';
#endif
        }
    }
}

void itb_ui_clear(itb_ui_context *ui_ctx) {
#if ITB_UI_UNICODE
    wmemset(ui_ctx->double_buff[0][0], L' ', ui_ctx->cols * ui_ctx->rows);
#else
    memset(ui_ctx->double_buff[0][0], ' ', ui_ctx->cols * ui_ctx->rows);
#endif
}

#if ITB_UI_UNICODE
int itb_ui_printf(itb_ui_context *ui_ctx, const wchar_t *fmt, ...) {
#else
int itb_ui_printf(itb_ui_context *ui_ctx, const char *fmt, ...) {
#endif
    va_list args;
    int ret;
    va_start(args, fmt);
#if ITB_UI_UNICODE
    ret = vswprintf(ui_ctx->render_line, ui_ctx->cols - ui_ctx->cursor[1] + 1, fmt, args);
#else
    ret = vsnprintf(ui_ctx->render_line, ui_ctx->cols - ui_ctx->cursor[1] + 1, fmt, args);
#endif

    if (ret > 0) {
        if ((size_t)ret > ui_ctx->cols - ui_ctx->cursor[1] + 1) {
            ret = ui_ctx->cols - ui_ctx->cursor[1] + 1;
        }
        itb_ui_dirty_box(ui_ctx, ui_ctx->cursor[0], ui_ctx->cursor[1], ui_ctx->cursor[0],
            ui_ctx->cursor[1] + ret);

#if ITB_UI_UNICODE
        wmemcpy(ui_ctx->double_buff[0][ui_ctx->cursor[0]] + ui_ctx->cursor[1], ui_ctx->render_line,
            ret);
#else
        memcpy(ui_ctx->double_buff[0][ui_ctx->cursor[0]] + ui_ctx->cursor[1], ui_ctx->render_line,
            ret);
#endif
    }
    va_end(args);
    return ret;
}

#if ITB_UI_UNICODE
int itb_ui_rcprintf(itb_ui_context *ui_ctx, size_t row, size_t col, const wchar_t *fmt, ...) {
#else
int itb_ui_rcprintf(itb_ui_context *ui_ctx, size_t row, size_t col, const char *fmt, ...) {
#endif
    if (row && row <= ui_ctx->rows && col && col <= ui_ctx->cols) {
        va_list args;
        int ret;
        va_start(args, fmt);
#if ITB_UI_UNICODE
        ret = vswprintf(ui_ctx->render_line, ui_ctx->cols - col + 1, fmt, args);
#else
        ret = vsnprintf(ui_ctx->render_line, ui_ctx->cols - col + 1, fmt, args);
#endif

        if (ret > 0) {
            if ((size_t)ret > ui_ctx->cols - col + 1) {
                ret = ui_ctx->cols - col + 1;
            }

            itb_ui_dirty_box(ui_ctx, row, col, row, col + ret);

#if ITB_UI_UNICODE
            wmemcpy(ui_ctx->double_buff[0][row] + col, ui_ctx->render_line, ret);
#else
            memcpy(ui_ctx->double_buff[0][row] + col, ui_ctx->render_line, ret);
#endif
        }
        va_end(args);
        return ret;
    } else {
        return -1;
    }
}

#endif //ITB_UI_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
