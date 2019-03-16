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
    mode1.set.fg = ITB_WHITE;
    mode1.set.bg = ITB_BLACK;

    mode2.set.fg = ITB_WHITE;
    mode2.set.bg = ITB_MAGENTA;

    for (int i = 0; i < 500; ++i) {
        if (i % 2) {
            itb_ui_color(&ctx, &mode1);
        } else {
            itb_ui_color(&ctx, &mode2);
        }
        itb_ui_rcprintf(&ctx, (i % ctx.rows), (i % ctx.cols), ITB_T("<%d>"), i);
        itb_ui_flip(&ctx);
    }

    /*
    itb_ui_clear(&ctx);
    itb_ui_flip(&ctx);

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
    */
    itb_ui_end(&ctx);
    return 0;
}
