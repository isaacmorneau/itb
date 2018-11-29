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

void test_callback(void * unused) {
    (void)unused;
    puts("test message");
}

void test_tls(void * unused) {
    (void)unused;
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

void test_vector(void * unused) {
    (void)unused;
    itb_vector_t vec;
    int tmp = 200;

    itb_vector_init(&vec, sizeof(int));

    for (int i = 0; i < 10; ++i) {
        itb_vector_push(&vec, &i);
    }
    puts("built");

    for (int i = 0; i < 10; ++i) {
        int *j;
        j = itb_vector_at(&vec, i);
        printf("vec %d @ %d\n", *j, i);
    }

    itb_vector_remove_at(&vec, 5);

    puts("remove at");
    for (int i = 0; i < 9; ++i) {
        int *j;
        j = itb_vector_at(&vec, i);
        printf("vec %d @ %d\n", *j, i);
    }

    itb_vector_push(&vec, &tmp);
    puts("pushed again");
    for (int i = 0; i < 10; ++i) {
        int *j;
        j = itb_vector_at(&vec, i);
        printf("vec %d @ %d\n", *j, i);
    }

    itb_vector_pop(&vec);

    puts("popped");
    for (int i = 0; i < 9; ++i) {
        int *j;
        j = itb_vector_at(&vec, i);
        printf("vec %d @ %d\n", *j, i);
    }

    itb_vector_close(&vec);
}

int main(void) {
    itb_menu_t mainmenu, submenu, subsubmenu;
    bool toggle = 0;

    itb_menu_init(&mainmenu, "main menu");
    itb_menu_init(&submenu, "sub menu");
    itb_menu_init(&subsubmenu, "sub sub menu");

    itb_menu_register_items(&submenu, itb_menu_item_menu("testing sub sub menu", &subsubmenu), NULL);

    itb_menu_register_items(&mainmenu, itb_menu_item_label("testing label"),
        itb_menu_item_callback("testing callback", test_callback, NULL),
        itb_menu_item_callback("testing tls", test_tls, NULL),
        itb_menu_item_callback("testing itb_vector", test_vector, NULL),
        itb_menu_item_menu("testing sub menu", &submenu),
        itb_menu_item_toggle("testing toggle", &toggle), NULL);

    itb_menu_run(&mainmenu);

    printf("final toggle value: %c\n", toggle ? 't' : 'f');

    puts("line by line");

    char buff[512];
    int ret;
    while (1) {
        itb_menu_print(&mainmenu);
invalid:
        printf("> ");
        fflush(stdout);
        itb_readline((unsigned char *)buff, 512);
        if ((ret = itb_menu_run_once(&mainmenu, buff)) == -1) {
            goto invalid;
        } else if (ret == 1) {
            break;
        }
        //else continue
    }

    itb_menu_close(&mainmenu);

    puts("finished");
}
