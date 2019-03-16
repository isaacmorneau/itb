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

    for (int i = 0; i < 100000; ++i) {
        mode1.set.fg = i % 6 + 1;
        mode1.set.bg = (mode1.set.fg + 1) % 7 + 1;
        itb_ui_color(&ctx, &mode1);

        itb_ui_rcprintf(&ctx, i % ctx.rows + 1, i % ctx.cols + 1, ITB_T("$"));
        itb_ui_flip(&ctx);
    }

    mode1.set.fg = ITB_WHITE;
    mode1.set.bg = ITB_BLACK;

    mode2.set.fg = ITB_WHITE;
    mode2.set.bg = ITB_MAGENTA;

    for (int i = 0; i < 1000; i += 10) {
        size_t r = (i % (ctx.rows - 11) + 1), c = (i % (ctx.cols - 11) + 1);
        itb_ui_color(&ctx, &mode2);
        itb_ui_box(&ctx, r, c, 11, 11);
        itb_ui_color(&ctx, &mode1);
        itb_ui_rcprintf(&ctx, r + 5, c + 4, ITB_T("<%d>"), i);
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
            itb_ui_rcprintf(&ctx, r + 4, c + 3, ITB_T("r:%d"), r + 4);
            itb_ui_rcprintf(&ctx, r + 6, c + 3, ITB_T("c:%d"), c + 3);
        }
    }
    itb_ui_flip(&ctx);

    itb_ui_show(&ctx);
    itb_ui_end(&ctx);
    return 0;
}
