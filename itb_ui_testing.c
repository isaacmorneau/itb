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

    for (int i = 0; i < 10000; ++i) {
        itb_ui_box(&ctx, (i % ctx.rows) + 1, (i % ctx.cols) + 1, 10, 10);
        itb_ui_rcprintf(&ctx, (i % ctx.rows) + 5, (i % ctx.cols) + 5, ITB_T("<%d>"), i);
        itb_ui_flip(&ctx);
    }

    itb_ui_clear(&ctx);
    itb_ui_flip_force(&ctx);

    for (size_t r = 0; r < ctx.rows; r += 10) {
        for (size_t c = 0; c < ctx.cols; c += 10) {
            itb_ui_box(&ctx, r+1, c+1, 9, 9);
        }
    }
    itb_ui_flip(&ctx);

    itb_ui_show(&ctx);
    itb_ui_end(&ctx);
    return 0;
}
