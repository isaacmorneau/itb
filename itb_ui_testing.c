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

    for (int i = 0; i < 5; ++i) {
        itb_ui_clear(&ctx);
        itb_ui_box(&ctx, (i%20)+5, (i%20)+5, 20, 20);
        itb_ui_flip(&ctx);
        sleep(1);
    }

    itb_ui_clear(&ctx);
    itb_ui_box(&ctx, 5, 5, 10, 10);
    itb_ui_flip(&ctx);

    itb_ui_end(&ctx);
    return 0;
}
