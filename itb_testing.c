#include <stdio.h>

#include "itb.h"
#define ITB_IMPLEMENTATION
#include "itb.h"
#include "itb_ui.h"
#define ITB_UI_IMPLEMENTATION
#include "itb_ui.h"

void test_callback(void) {
    puts("test message");
}

int main(void) {
    itb_menu_t mainmenu, submenu;
    bool toggle = 0;

    itb_menu_init(&mainmenu, "main menu");
    itb_menu_init(&submenu, "sub menu");

    itb_menu_register_items(&mainmenu, itb_menu_item_label("testing label"),
        itb_menu_item_callback("testing callback", test_callback),
        itb_menu_item_menu("testing sub menu", &submenu),
        itb_menu_item_toggle("testing toggle", &toggle), NULL);

    itb_menu_run(&mainmenu);

    printf("final toggle value: %c\n", toggle ? 't' : 'f');

    itb_menu_close(&mainmenu);

    puts("finished");
}
