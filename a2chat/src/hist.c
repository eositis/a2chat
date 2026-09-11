#include "a2chat.h"
#include <string.h>
#include <stdio.h>

#ifndef A2CHAT_HOST
#pragma code-name ("LC")
#endif

#define DEFAULT_HISTCAP 0x7FFFu
#define DEFAULT_MAXHIST 4096u

#ifndef A2CHAT_HOST
static const char *hist_path(void)
{
    return self_path("A2CHAT.LOG");
}
#else
static const char *hist_path(void)
{
    return "A2CHAT.LOG";
}
#endif

static char iobuf[128];

long hist_size(void)
{
    FILE *f;
    long n;
    f = fopen(hist_path(), "rb");
    if (!f) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fclose(f);
    return n;
}

static int skip_record_payload(FILE *f, uint16_t len)
{
    while (len) {
        uint16_t n = len;
        if (n > sizeof(iobuf)) {
            n = (uint16_t)sizeof(iobuf);
        }
        if (fread(iobuf, 1, n, f) != n) {
            return -1;
        }
        len = (uint16_t)(len - n);
    }
    return 0;
}

static int hist_compact(void)
{
    FILE *in;
    FILE *out;
    long sz;
    long pos;
    long skip;
    uint16_t cap;
    size_t n;
    int t;
    unsigned lo;
    unsigned hi;
    uint16_t len;

    cap = g_cfg.histcap ? g_cfg.histcap : DEFAULT_HISTCAP;
    sz = hist_size();
    if (sz <= (long)cap) {
        return 0;
    }
    skip = sz - (long)cap;
    in = fopen(hist_path(), "rb");
    if (!in) {
        return -1;
    }
    pos = 0;
    while (pos < skip) {
        t = fgetc(in);
        if (t != 'U' && t != 'A' && t != 'T') {
            break;
        }
        lo = (unsigned)fgetc(in);
        hi = (unsigned)fgetc(in);
        if (lo == (unsigned)EOF || hi == (unsigned)EOF) {
            break;
        }
        len = (uint16_t)(lo | (hi << 8));
        if (skip_record_payload(in, len) < 0) {
            break;
        }
        pos += 3 + (long)len;
    }
#ifndef A2CHAT_HOST
    out = fopen(self_path("A2CHAT.HC"), "wb");
#else
    out = fopen("A2CHAT.HC", "wb");
#endif
    if (!out) {
        fclose(in);
        return -1;
    }
    while ((n = fread(iobuf, 1, sizeof(iobuf), in)) > 0) {
        fwrite(iobuf, 1, n, out);
    }
    fclose(in);
    fclose(out);
    {
        static char hc[80];
        strcpy(hc, self_path("A2CHAT.HC"));
        remove(hist_path());
        rename(hc, hist_path());
    }
    return 0;
}

static int hist_write_hdr(FILE *f, char type, uint16_t len)
{
    unsigned char hdr[3];

    hdr[0] = (unsigned char)type;
    hdr[1] = (unsigned char)(len & 0xff);
    hdr[2] = (unsigned char)(len >> 8);
    return fwrite(hdr, 1, 3, f) == 3 ? 0 : -1;
}

static void hist_maybe_compact(void)
{
    if (hist_size() > (long)g_cfg.histcap) {
        hist_compact();
    }
}

int hist_append(char type, const char *data, uint16_t len)
{
    FILE *f;

    f = fopen(hist_path(), "ab");
    if (!f) {
        f = fopen(hist_path(), "wb");
        if (!f) {
            return -1;
        }
    }
    if (hist_write_hdr(f, type, len) < 0) {
        fclose(f);
        return -1;
    }
    if (len) {
        fwrite(data, 1, len, f);
    }
    fclose(f);
    hist_maybe_compact();
    return 0;
}

int hist_append_aux(char type, uint16_t len)
{
    FILE *out;
    uint16_t off;

    if (len == 0) {
        return 0;
    }
    out = fopen(hist_path(), "ab");
    if (!out) {
        out = fopen(hist_path(), "wb");
        if (!out) {
            return -1;
        }
    }
    if (hist_write_hdr(out, type, len) < 0) {
        fclose(out);
        return -1;
    }
    off = 0;
    while (off < len) {
        uint16_t n = sizeof(iobuf);
        if (n > (uint16_t)(len - off)) {
            n = (uint16_t)(len - off);
        }
        aux_read(off, (unsigned char *)iobuf, n);
        fwrite(iobuf, 1, n, out);
        off += n;
    }
    fclose(out);
    hist_maybe_compact();
    return 0;
}

void hist_new(void)
{
    FILE *f = fopen(hist_path(), "wb");
    if (f) {
        fclose(f);
    }
}

int hist_write_messages(FILE *body)
{
    FILE *f;
    long sz;
    long pos;
    long start;
    uint16_t budget;

    f = fopen(hist_path(), "rb");
    if (!f) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    budget = g_cfg.maxhist ? g_cfg.maxhist : DEFAULT_MAXHIST;
    start = 0;
    if (sz > (long)budget) {
        start = sz - (long)budget;
    }
    fseek(f, 0, SEEK_SET);
    pos = 0;
    for (;;) {
        int t;
        unsigned lo;
        unsigned hi;
        uint16_t len;
        uint16_t left;
        int emit;

        t = fgetc(f);
        if (t == EOF) {
            break;
        }
        if (t != 'U' && t != 'A' && t != 'T') {
            break;
        }
        lo = (unsigned)fgetc(f);
        hi = (unsigned)fgetc(f);
        if (lo == (unsigned)EOF || hi == (unsigned)EOF) {
            break;
        }
        len = (uint16_t)(lo | (hi << 8));
        /* Replay user and assistant turns. Skip tool dumps and the old
         * "(streamed)" markers that had no real reply text. */
        emit = ((t == 'U' || t == 'A') && pos >= start);
        pos += 3 + (long)len;
        if (t == 'A' && len == 10) {
            if (fread(iobuf, 1, 10, f) != 10) {
                break;
            }
            if (!memcmp(iobuf, "(streamed)", 10)) {
                continue;
            }
            if (emit) {
                fputc(',', body);
                fputs("{\"role\":\"assistant\",\"content\":\"", body);
                json_escape_fwrite(body, iobuf, 10);
                fputs("\"}", body);
            }
            continue;
        }
        if (!emit) {
            if (skip_record_payload(f, len) < 0) {
                break;
            }
            continue;
        }
        fputc(',', body);
        fputs(t == 'A' ? "{\"role\":\"assistant\",\"content\":\""
                       : "{\"role\":\"user\",\"content\":\"", body);
        left = len;
        while (left) {
            uint16_t n = left;
            if (n > sizeof(iobuf)) {
                n = (uint16_t)sizeof(iobuf);
            }
            if (fread(iobuf, 1, n, f) != n) {
                break;
            }
            json_escape_fwrite(body, iobuf, n);
            left = (uint16_t)(left - n);
        }
        fputs("\"}", body);
    }
    fclose(f);
    return 0;
}
