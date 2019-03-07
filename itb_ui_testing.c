#include <unistd.h>

#include "itb_ui.h"
#define ITB_UI_IMPLEMENTATION
#include "itb_ui.h"

int main(void) {
    itb_ui_context ctx;
    if (itb_ui_start(&ctx)) {
        perror("start");
        return 1;
    }

    for (int i = 0; i < 10000; ++i) {
        itb_ui_box(&ctx, (i % ctx.rows) + 1, (i % ctx.cols) + 1, 10, 10);
        itb_ui_rcprintf(&ctx, (i % ctx.rows) + 1, (i % ctx.cols) + 1, L"<%d>", i);
        itb_ui_flip(&ctx);
    }
    for (int r = 1; r < ctx.rows; ++r) {
        for (int c = 1; c < ctx.cols; ++c) {
            itb_ui_rcprintf(&ctx, r, c, L"Ʊ");
            itb_ui_flip(&ctx);
        }
    }

    itb_ui_end(&ctx);
    return 0;
}
