#include "a2chat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include <unistd.h>
#include <device.h>
#include <cc65.h>
#include <ip65.h>

static char pathbuf[80];
static char progdir[80];
static char line[A2CHAT_LINE_MAX];

char *self_path(const char *filename)
{
    const char *dir;
    unsigned n;

    dir = g_cfg.prefix[0] ? g_cfg.prefix : progdir;
    pathbuf[0] = 0;
    if (dir[0]) {
        strncpy(pathbuf, dir, sizeof(pathbuf) - 1);
        pathbuf[sizeof(pathbuf) - 1] = 0;
        n = (unsigned)strlen(pathbuf);
        if (n && pathbuf[n - 1] != '/' && n + 1 < sizeof(pathbuf)) {
            pathbuf[n++] = '/';
            pathbuf[n] = 0;
        }
    }
    strncat(pathbuf, filename, sizeof(pathbuf) - 1 - strlen(pathbuf));
    return pathbuf;
}

static void reset_cwd(void)
{
    chdir("");
}

int main(int argc, char *argv[])
{
    char *slash;
    int cfg_ok;
    static uint8_t first_chat = 1;

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

    if (progdir[0]) {
        chdir(progdir);
        atexit(reset_cwd);
    } else {
        char cwd[64];
        if (!*getcwd(cwd, sizeof(cwd))) {
            chdir(getdevicedir(getcurrentdevice(), cwd, sizeof(cwd)));
            atexit(reset_cwd);
        }
    }

    cfg_ok = cfg_load_first(&g_cfg);
    if (g_cfg.prefix[0]) {
        if (chdir(g_cfg.prefix) != 0) {
            /* keep progdir cwd; tools still use absolute PREFIX paths */
        }
    }
    g_slot = slot_resolve(g_cfg.slot);
    ui_init();
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
    net_diag_ollama();

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
    return 0;
}
