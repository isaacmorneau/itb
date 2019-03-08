#include <unistd.h>

#define ITB_UI_IMPLEMENTATION
//#define ITB_UI_UNICODE 0
#include "itb_ui.h"

int main(void) {
    itb_ui_context ctx;
    if (itb_ui_start(&ctx)) {
        perror("start");
        return 1;
    }

    itb_ui_hide(&ctx);

    for (int i = 0; i < 10000; ++i) {
        itb_ui_box(&ctx, (i % ctx.rows) + 1, (i % ctx.cols) + 1, 10, 10);
        itb_ui_rcprintf(&ctx, (i % ctx.rows) + 1, (i % ctx.cols) + 1, ITB_T("<%d>"), i);
        itb_ui_flip(&ctx);
    }

    itb_ui_clear(&ctx);
    itb_ui_flip(&ctx);

    for (size_t r = 1; r <= ctx.rows; ++r) {
        for (size_t c = 1; c <= ctx.cols; ++c) {
#if ITB_UI_UNICODE
            itb_ui_rcprintf(&ctx, r, c, L"Ʊ");
#else
            itb_ui_rcprintf(&ctx, r, c, "#");
#endif
            itb_ui_flip(&ctx);
        }
    }
    //ensure mov works as expected
    itb_ui_clear(&ctx);

    itb_ui_box(&ctx, 5, 5, ctx.cols - 10, ctx.rows - 10);

    itb_ui_rcprintf(
        &ctx, ctx.rows / 2, ctx.cols / 2, ITB_T("+ marks r:%lu c:%lu"), ctx.rows / 2, ctx.cols / 2);
    itb_ui_rcprintf(&ctx, 3, 3, ITB_T("+ marks r:%lu c:%lu"), 3, 3);

    itb_ui_flip(&ctx);

#if 0
    //this is only enabled to test hard updates
    itb_ui_clear(&ctx);
    itb_ui_flip(&ctx);

    for (size_t r = 1; r < ctx.rows; ++r) {
        for (size_t c = 1; c < ctx.cols; ++c) {
            itb_ui_rcprintf(&ctx, r, c, L"Ʊ");
            itb_ui_flip_force(&ctx);
        }
    }
#endif

    itb_ui_show(&ctx);
    itb_ui_end(&ctx);
    return 0;
}
