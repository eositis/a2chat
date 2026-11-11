#include "a2chat.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

struct a2cfg g_cfg;
uint8_t g_stream_set;
#ifndef A2CHAT_HOST
char g_cfg_loaded[A2CHAT_PATH_MAX];
/* Parser and loader sit in the NIC hole. nic_bringup overwrites them
   after cfg_load_first returns. cfg_save and cfg_api_path stay resident. */
#pragma code-name (push, "CFGINIT")
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
    c->stream = A2CHAT_STREAM_DEFAULT;
    g_stream_set = 0;
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
    } else if (keyeq(buf, "STREAM")) {
        c->stream = (uint8_t)(atoi(val) != 0);
        g_stream_set = 1;
    } else if (keyeq(buf, "API")) {
        if (keyeq(val, "OPENAI") || keyeq(val, "LLAMA") ||
            keyeq(val, "CPP") || keyeq(val, "LLAMACPP")) {
            c->api = A2CHAT_API_OPENAI;
        } else if (keyeq(val, "OLLA")) {
            c->api = A2CHAT_API_OLLA;
        } else if (keyeq(val, "OLLAMA")) {
            c->api = A2CHAT_API_OLLAMA;
        }
    }
}

#ifndef A2CHAT_HOST
#pragma code-name (pop)
#endif

uint8_t g_api_lock;

int cfg_is_olla(const struct a2cfg *c)
{
    return c != 0 && c->api == A2CHAT_API_OLLA;
}

int cfg_use_openai(const struct a2cfg *c)
{
    uint8_t api;

    if (!c) {
        return 1;
    }
    api = c->api ? c->api : g_api_lock;
    return api != A2CHAT_API_OLLAMA && api != A2CHAT_API_OLLA;
}

const char *cfg_api_path(const struct a2cfg *c, const char *suffix)
{
    static char path[40];

    if (!suffix) {
        suffix = "/api/chat";
    }
    if (cfg_use_openai(c)) {
        if (strcmp(suffix, "/api/tags") == 0) {
            return "/v1/models";
        }
        return "/v1/chat/completions";
    }
    if (cfg_is_olla(c)) {
        strcpy(path, "/olla/ollama");
        strcat(path, suffix);
        return path;
    }
    return suffix;
}

#ifndef A2CHAT_HOST
#pragma code-name (push, "CFGINIT")
#endif

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

#ifndef A2CHAT_HOST
#pragma code-name (pop)

static void cfg_line_s(FILE *f, const char *key, const char *val)
{
    fputs(key, f);
    if (val) {
        fputs(val, f);
    }
    fputc('\r', f);
}

static void cfg_line_u(FILE *f, const char *key, unsigned val)
{
    char num[6];

    fputs(key, f);
    fmt_start(num, sizeof(num));
    fmt_u(val);
    fmt_end();
    fputs(num, f);
    fputc('\r', f);
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
    fprintf(f, "STREAM=%u\n", (unsigned)c->stream);
    if (c->api == A2CHAT_API_OPENAI) {
        fprintf(f, "API=OPENAI\n");
    } else if (c->api == A2CHAT_API_OLLAMA) {
        fprintf(f, "API=OLLAMA\n");
    } else if (c->api == A2CHAT_API_OLLA) {
        fprintf(f, "API=OLLA\n");
    }
#else
    cfg_line_s(f, "HOST=", c->host);
    cfg_line_u(f, "PORT=", c->port);
    cfg_line_s(f, "MODEL=", c->model);
    cfg_line_u(f, "SLOT=", c->slot);
    cfg_line_s(f, "IP=", c->ip);
    cfg_line_s(f, "GATEWAY=", c->gateway);
    cfg_line_s(f, "NETMASK=", c->netmask);
    cfg_line_s(f, "PREFIX=", c->prefix);
    cfg_line_u(f, "MAXHIST=", c->maxhist);
    cfg_line_u(f, "MAXREAD=", c->maxread);
    cfg_line_u(f, "MAXWRITE=", c->maxwrite);
    cfg_line_u(f, "HISTCAP=", c->histcap);
    cfg_line_u(f, "STREAM=", c->stream);
    if (c->api == A2CHAT_API_OPENAI) {
        cfg_line_s(f, "API=OPENAI", "");
    } else if (c->api == A2CHAT_API_OLLAMA) {
        cfg_line_s(f, "API=OLLAMA", "");
    } else if (c->api == A2CHAT_API_OLLA) {
        cfg_line_s(f, "API=OLLA", "");
    }
#endif
    fclose(f);
    return 0;
}
