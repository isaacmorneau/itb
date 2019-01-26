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

    size_t pos[2][2];

    pos[0][0] = 10;
    pos[0][1] = 5;
    pos[1][0] = 5;
    pos[1][1] = 10;

    size_t size[2] = {20, 20};

    for (int i = 0; i < 4; ++i) {
        itb_ui_box(&ctx, pos[i%2], size);
        itb_ui_flip(&ctx);
        sleep(1);
    }

    itb_ui_end(&ctx);
    return 0;
}
