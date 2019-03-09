#include <unistd.h>

#define ITB_UI_IMPLEMENTATION
#define ITB_UI_UNICODE 0
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
        //usleep(3000);
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

    for (size_t r = 1; r <= ctx.rows; ++r) {
        for (size_t c = 1; c <= ctx.cols; c += 2) {
            itb_ui_rcprintf(&ctx, r, c, ITB_T("|"));
        }
    }

    itb_ui_flip(&ctx);

    itb_ui_show(&ctx);
    itb_ui_end(&ctx);
    return 0;
}
