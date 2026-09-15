#include "a2chat.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

struct a2cfg g_cfg;
#ifndef A2CHAT_HOST
char g_cfg_loaded[A2CHAT_PATH_MAX];
#endif

static void ascii7(char *s)
{
    while (*s) {
        *s = (char)((unsigned char)*s & 0x7f);
        s++;
    }
}

static void trim(char *s)
{
    char *e;
    ascii7(s);
    while (*s == ' ' || *s == '\t') {
        memmove(s, s + 1, strlen(s));
    }
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' ||
                     e[-1] == '\n')) {
        *--e = 0;
    }
}

static int keyeq(const char *a, const char *b)
{
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

void cfg_defaults(struct a2cfg *c)
{
    memset(c, 0, sizeof(*c));
    strcpy(c->host, "192.168.1.10");
    c->port = 11434;
    strcpy(c->model, "llama3.1");
    c->slot = 0;
    c->prefix[0] = 0;
    c->maxhist = 4096;
    c->maxread = 4096;
    c->maxwrite = 0xFFFFu;
    c->histcap = 0x7FFFu;
}

void cfg_parse_line(struct a2cfg *c, const char *line)
{
    char buf[96];
    char *eq;
    char *val;

    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    trim(buf);
    if (buf[0] == 0 || buf[0] == '#') {
        return;
    }
    eq = strchr(buf, '=');
    if (!eq) {
        return;
    }
    *eq = 0;
    val = eq + 1;
    trim(buf);
    trim(val);
    if (keyeq(buf, "HOST")) {
        strncpy(c->host, val, sizeof(c->host) - 1);
        c->host[sizeof(c->host) - 1] = 0;
    } else if (keyeq(buf, "PORT")) {
        c->port = (uint16_t)atoi(val);
    } else if (keyeq(buf, "MODEL")) {
        strncpy(c->model, val, sizeof(c->model) - 1);
        c->model[sizeof(c->model) - 1] = 0;
    } else if (keyeq(buf, "SLOT")) {
        c->slot = (uint8_t)atoi(val);
    } else if (keyeq(buf, "IP")) {
        strncpy(c->ip, val, sizeof(c->ip) - 1);
    } else if (keyeq(buf, "GATEWAY")) {
        strncpy(c->gateway, val, sizeof(c->gateway) - 1);
    } else if (keyeq(buf, "NETMASK")) {
        strncpy(c->netmask, val, sizeof(c->netmask) - 1);
    } else if (keyeq(buf, "PREFIX")) {
        strncpy(c->prefix, val, sizeof(c->prefix) - 1);
    } else if (keyeq(buf, "MAXHIST")) {
        c->maxhist = (uint16_t)atoi(val);
    } else if (keyeq(buf, "MAXREAD")) {
        c->maxread = (uint16_t)atoi(val);
    } else if (keyeq(buf, "MAXWRITE")) {
        c->maxwrite = (uint16_t)atoi(val);
    } else if (keyeq(buf, "HISTCAP")) {
        c->histcap = (uint16_t)atoi(val);
    }
}

static FILE *cfg_open(const char *path)
{
    FILE *f;
    f = fopen(path, "rb");
    if (f) {
        return f;
    }
    return fopen(path, "r");
}

int cfg_load(struct a2cfg *c, const char *path)
{
    FILE *f;
    char line[80];
    unsigned n;
    int ch;
    int cr;

    cfg_defaults(c);
    f = cfg_open(path);
    if (!f) {
        return -1;
    }
    n = 0;
    cr = 0;
    while ((ch = fgetc(f)) != EOF) {
        ch &= 0x7f;
        if (ch == '\n' && cr) {
            cr = 0;
            continue;
        }
        cr = 0;
        if (ch == '\r' || ch == '\n') {
            if (ch == '\r') {
                cr = 1;
            }
            line[n] = 0;
            cfg_parse_line(c, line);
            n = 0;
            continue;
        }
        if (n + 1 < sizeof(line)) {
            line[n++] = (char)ch;
        }
    }
    if (n) {
        line[n] = 0;
        cfg_parse_line(c, line);
    }
    fclose(f);
    if (c->port == 0) {
        c->port = 11434;
    }
    if (c->maxhist == 0) {
        c->maxhist = 4096;
    }
    if (c->maxread == 0) {
        c->maxread = 4096;
    }
    if (c->maxwrite == 0) {
        c->maxwrite = 0xFFFFu;
    }
    if (c->histcap == 0) {
        c->histcap = 0x7FFFu;
    }
    if (c->prefix[0] && c->prefix[0] != '/') {
        memmove(c->prefix + 1, c->prefix, strlen(c->prefix) + 1);
        c->prefix[0] = '/';
    }
    {
        unsigned n = (unsigned)strlen(c->prefix);
        while (n > 1 && c->prefix[n - 1] == '/') {
            c->prefix[--n] = 0;
        }
    }
    return 0;
}

#ifndef A2CHAT_HOST
int cfg_load_first(struct a2cfg *c)
{
    const char *try_path[3];
    unsigned i;

    try_path[0] = "A2CHAT.CFG";
    try_path[1] = "A2CHAT.CF";
    try_path[2] = 0;

    g_cfg_loaded[0] = 0;
    for (i = 0; try_path[i]; i++) {
        strncpy(g_cfg_loaded, try_path[i], sizeof(g_cfg_loaded) - 1);
        g_cfg_loaded[sizeof(g_cfg_loaded) - 1] = 0;
        if (cfg_load(c, g_cfg_loaded) == 0) {
            return 0;
        }
    }
    g_cfg_loaded[0] = 0;
    cfg_defaults(c);
    return -1;
}
#endif

int cfg_save(const struct a2cfg *c, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        return -1;
    }
#ifdef A2CHAT_HOST
    fprintf(f, "HOST=%s\n", c->host);
    fprintf(f, "PORT=%u\n", (unsigned)c->port);
    fprintf(f, "MODEL=%s\n", c->model);
    fprintf(f, "SLOT=%u\n", (unsigned)c->slot);
    fprintf(f, "IP=%s\n", c->ip);
    fprintf(f, "GATEWAY=%s\n", c->gateway);
    fprintf(f, "NETMASK=%s\n", c->netmask);
    fprintf(f, "PREFIX=%s\n", c->prefix);
    fprintf(f, "MAXHIST=%u\n", (unsigned)c->maxhist);
    fprintf(f, "MAXREAD=%u\n", (unsigned)c->maxread);
    fprintf(f, "MAXWRITE=%u\n", (unsigned)c->maxwrite);
    fprintf(f, "HISTCAP=%u\n", (unsigned)c->histcap);
#else
    fprintf(f, "HOST=%s\r", c->host);
    fprintf(f, "PORT=%u\r", (unsigned)c->port);
    fprintf(f, "MODEL=%s\r", c->model);
    fprintf(f, "SLOT=%u\r", (unsigned)c->slot);
    fprintf(f, "IP=%s\r", c->ip);
    fprintf(f, "GATEWAY=%s\r", c->gateway);
    fprintf(f, "NETMASK=%s\r", c->netmask);
    fprintf(f, "PREFIX=%s\r", c->prefix);
    fprintf(f, "MAXHIST=%u\r", (unsigned)c->maxhist);
    fprintf(f, "MAXREAD=%u\r", (unsigned)c->maxread);
    fprintf(f, "MAXWRITE=%u\r", (unsigned)c->maxwrite);
    fprintf(f, "HISTCAP=%u\r", (unsigned)c->histcap);
#endif
    fclose(f);
    return 0;
}
