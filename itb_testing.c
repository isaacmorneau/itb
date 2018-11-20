#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "itb.h"
#define ITB_IMPLEMENTATION
#include "itb.h"
#include "itb_ui.h"
#define ITB_UI_IMPLEMENTATION
#include "itb_ui.h"

#define ITB_SSL_ADDITIONS
#include "itb_net.h"
#define ITB_NET_IMPLEMENTATION
#include "itb_net.h"

void test_callback(void) {
    puts("test message");
}

void test_tls(void) {
    itb_ssl_conn_t conn;

    if (itb_ssl_init(&conn, "example.com")) {
        puts("ssl init failed");
    }

    unsigned char buff[1024] = "GET / HTTP/1.1\r\n"
                      "Host: example.com:443\r\n"
                      "Connection: Close\r\n"
                      "\r\n";
    itb_ssl_write(&conn, buff, strlen((const char*)buff));

    int nread;
    while ((nread = itb_ssl_read(&conn, buff, sizeof(buff))) > 0) {
        printf("%.*s\n", nread, buff);
    }

    itb_ssl_cleanup(&conn);
}

int main(void) {
    itb_menu_t mainmenu, submenu;
    bool toggle = 0;

    itb_menu_init(&mainmenu, "main menu");
    itb_menu_init(&submenu, "sub menu");

    itb_menu_register_items(&mainmenu, itb_menu_item_label("testing label"),
        itb_menu_item_callback("testing callback", test_callback),
        itb_menu_item_callback("testing tls", test_tls),
        itb_menu_item_menu("testing sub menu", &submenu),
        itb_menu_item_toggle("testing toggle", &toggle), NULL);

    itb_menu_run(&mainmenu);

    printf("final toggle value: %c\n", toggle ? 't' : 'f');

    itb_menu_close(&mainmenu);

    puts("finished");
}
