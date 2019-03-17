#include <unistd.h>

#define ITB_UI_IMPLEMENTATION
//#define ITB_UI_UNICODE 0
#include "itb_ui.h"

int main(void) {
    itb_ui_context ctx;

    int ret;
    if ((ret = itb_ui_start(&ctx))) {
        perror("start");
        printf("%d\n", ret);
        return 1;
    }

    itb_ui_hide(&ctx);

    itb_color_mode mode1, mode2;

    itb_ui_stash sh[10];

    for (int n = 0; n < 10; ++n) {
        itb_ui_stash_init(&ctx, sh + n);
    }


    for (int n = 0; n < 10; ++n) {
        for (int i = 0; i < 100000; ++i) {
            mode1.set.fg = i % 6 + 1;
            mode1.set.bg = (mode1.set.fg + 1 + n) % 7 + 1;
            itb_ui_color(&ctx, &mode1);

            itb_ui_strcpy(&ctx, i % ctx.rows + 1, i % ctx.cols + 1, ITB_T("$"), 1);
            itb_ui_flip(&ctx);
        }
        itb_ui_stash_copy(&ctx, sh + n);

        mode1.set.fg = ITB_WHITE;
        mode1.set.bg = ITB_BLACK;

        mode2.set.fg = ITB_WHITE;
        mode2.set.bg = ITB_MAGENTA;

        for (int i = n; i < 1000; i += 10) {
            size_t r = (i % (ctx.rows - 11) + 1), c = (i % (ctx.cols - 11) + 1);
            itb_ui_color(&ctx, &mode2);
            itb_ui_box(&ctx, r, c, 11, 11);
            itb_ui_color(&ctx, &mode1);
            itb_ui_printf(&ctx, r + 5, c + 4, ITB_T("<%d>"), i);
            itb_ui_flip(&ctx);
        }

        //itb_ui_clear(&ctx);
        itb_ui_flip(&ctx);
        mode1.set.fg = ITB_BLACK;
        mode1.set.bg = ITB_RED;
        itb_ui_color(&ctx, &mode1);
        for (size_t r = 0; r < ctx.rows; r += 10) {
            for (size_t c = 0; c < ctx.cols; c += 10) {
                //itb_ui_box(&ctx, r + 1, c + 1, 9, 9);
                itb_ui_printf(&ctx, r + 4, c + 3, ITB_T("r:%d"), r + 4);
                itb_ui_printf(&ctx, r + 6, c + 3, ITB_T("c:%d"), c + 3);
            }
        }
        itb_ui_flip(&ctx);
    }

    for (int n = 9; n >= 0; --n) {
        itb_ui_stash_paste(&ctx, sh + n);
        itb_ui_flip(&ctx);
    }

    itb_ui_show(&ctx);

    for (int n = 0; n < 10; ++n) {
        itb_ui_stash_close(sh + n);
    }

    itb_ui_end(&ctx);
    return 0;
}
