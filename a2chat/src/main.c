#include "a2chat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include <cc65.h>
#include <ip65.h>

static char pathbuf[A2CHAT_PATH_MAX];
static char progdir[A2CHAT_PATH_MAX];
static char line[A2CHAT_LINE_MAX];

char *self_path(const char *filename)
{
    /* App files live in the ProDOS launch directory (current prefix). */
    strncpy(pathbuf, filename, sizeof(pathbuf) - 1);
    pathbuf[sizeof(pathbuf) - 1] = 0;
    return pathbuf;
}

int main(int argc, char *argv[])
{
    char *slash;
    int cfg_ok;
    static uint8_t first_chat = 1;

#ifndef A2CHAT_HOST
    __asm__("cld");
    __asm__("sei");
#endif

    progdir[0] = 0;
    if (argc > 0 && argv[0] && argv[0][0]) {
        strncpy(progdir, argv[0], sizeof(progdir) - 1);
        progdir[sizeof(progdir) - 1] = 0;
        slash = strrchr(progdir, '/');
        if (slash) {
            *(slash + 1) = 0;
        } else {
            progdir[0] = 0;
        }
    }

    if (doesclrscrafterexit()) {
        /* pause on exit when launched from BASIC-ish */
    }

#ifndef A2CHAT_HOST
    if (!progdir[0]) {
        p8_prefix(progdir);
    }
#endif

    cfg_ok = cfg_load_first(&g_cfg);
    g_slot = slot_resolve(g_cfg.slot);
    ui_init();
    clock_init();
    ui_redraw_chrome();
    if (net_init(g_slot) != 0) {
        ui_print("Continuing offline. Slash commands still work.");
        ui_nl();
    }
    ui_redraw_chrome();
    if (cfg_ok == 0) {
        ui_print("Config: ");
        ui_print(g_cfg_loaded);
    } else {
        ui_print("Config: A2CHAT.CFG not found, using defaults");
    }
    ui_nl();
    ui_print("A2CHAT ready. Type a prompt or /help. /ping retests Ollama.");
    ui_nl();
    ui_print("Data ");
    ui_print(g_cfg.prefix[0] ? g_cfg.prefix : (progdir[0] ? progdir : "(cwd)"));
    ui_nl();
    ui_print("Clock ");
    ui_print(clock_kind_name());
    ui_print(" ");
    {
        char t[10];
        clock_wall(t, sizeof t);
        ui_print(t);
    }
    ui_nl();
    net_diag_ollama();
#ifndef A2CHAT_HOST
    __asm__("cld");
    __asm__("sei");
#endif

    for (;;) {
        ui_prompt(line, A2CHAT_LINE_MAX);
        if (!line[0]) {
            continue;
        }
        if (line[0] == '/') {
            if (cmd_handle(line) == 1) {
                break;
            }
            continue;
        }
        if (first_chat) {
            ui_clear_chat();
            first_chat = 0;
        }
        ollama_send(line, 0);
    }
    net_shutdown();
    ui_exit();
    prodos_quit();
    return 0;
}
