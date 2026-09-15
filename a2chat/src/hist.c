#include "a2chat.h"
#include <string.h>
#include <stdio.h>
#ifndef A2CHAT_HOST
#include <apple2_filetype.h>
#endif

/* hist uses fopen/MLI: must live in MAIN. LC $Dxxx is unmapped across
 * ProDOS and RTS lands in Applesoft ($D983 dump). */

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

#define iobuf g_io80

#ifndef A2CHAT_HOST
/* cc65 fopen("w") defaults to ProDOS BIN. Editors want TXT. */
static FILE *hist_fopen(const char *mode)
{
    _filetype = PRODOS_T_TXT;
    _auxtype = 0;
    return fopen(hist_path(), mode);
}

static void hist_mark_txt(void)
{
    p8_set_txt((char *)hist_path());
}
#endif

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

static int hist_compact(void)
{
    FILE *in;
    FILE *out;
    long sz;
    long skip;
    uint16_t cap;
    size_t n;
#ifdef A2CHAT_HOST
    long pos;
    int t;
    unsigned lo;
    unsigned hi;
    uint16_t len;
#endif

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
#ifndef A2CHAT_HOST
    {
        unsigned au = 0;
        uint16_t off;
        uint16_t chunk;

        if (skip > 0) {
            fseek(in, skip, SEEK_SET);
        }
        while ((n = fread(iobuf, 1, sizeof(iobuf), in)) > 0) {
            if (au + (unsigned)n > A2CHAT_AUX_POST_MAX) {
                break;
            }
            aux_write(au, (const unsigned char *)iobuf, (unsigned)n);
            au += (unsigned)n;
        }
        fclose(in);
        out = hist_fopen("wb");
        if (!out) {
            return -1;
        }
        off = 0;
        while (off < au) {
            chunk = (uint16_t)sizeof(iobuf);
            if ((unsigned)off + chunk > au) {
                chunk = (uint16_t)(au - off);
            }
            aux_read(off, (unsigned char *)iobuf, chunk);
            fwrite(iobuf, 1, chunk, out);
            off += chunk;
        }
        fclose(out);
        hist_mark_txt();
        return 0;
    }
#else
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
    out = fopen("A2CHAT.HC", "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    while ((n = fread(iobuf, 1, sizeof(iobuf), in)) > 0) {
        fwrite(iobuf, 1, n, out);
    }
    fclose(in);
    fclose(out);
#endif
    {
        strcpy(iobuf, self_path("A2CHAT.HC"));
        remove(hist_path());
        rename(iobuf, hist_path());
    }
    return 0;
}

#ifdef A2CHAT_HOST
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

static int hist_write_hdr(FILE *f, char type, uint16_t len)
{
    unsigned char hdr[3];

    hdr[0] = (unsigned char)type;
    hdr[1] = (unsigned char)(len & 0xff);
    hdr[2] = (unsigned char)(len >> 8);
    return fwrite(hdr, 1, 3, f) == 3 ? 0 : -1;
}
#endif

static void hist_maybe_compact(void)
{
    if (hist_size() > (long)g_cfg.histcap) {
        hist_compact();
    }
}

#ifndef A2CHAT_HOST
static FILE *hist_open_append(void)
{
    FILE *f;
    int c;

    f = fopen(hist_path(), "rb");
    if (!f) {
        return hist_fopen("wb");
    }
    c = fgetc(f);
    fclose(f);
    if (c != '>' && c != EOF) {
        return hist_fopen("wb");
    }
    return hist_fopen("ab");
}

static int hist_put_head(FILE *f, char type)
{
    char stamp[20];
    const char *tag;

    clock_stamp(stamp, sizeof stamp);
    if (type == 'A') {
        tag = ">AI ";
    } else if (type == 'T') {
        tag = ">TOOL ";
    } else {
        tag = ">YOU ";
    }
    fputs(tag, f);
    fputs(stamp, f);
    fputc('\r', f);
    return 0;
}
#endif

int hist_append(char type, const char *data, uint16_t len)
{
    FILE *f;

#ifndef A2CHAT_HOST
    __asm__("cld");
    f = hist_open_append();
    if (!f) {
        return -1;
    }
    hist_put_head(f, type);
    if (len) {
        fwrite(data, 1, len, f);
    }
    fputc('\r', f);
    fclose(f);
    hist_mark_txt();
    hist_maybe_compact();
    return 0;
#else

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
#endif
}

int hist_append_aux(char type, uint16_t len)
{
    FILE *out;
    uint16_t off;

#ifndef A2CHAT_HOST
    __asm__("cld");
#endif

    if (len == 0) {
        return 0;
    }
#ifndef A2CHAT_HOST
    out = hist_open_append();
    if (!out) {
        return -1;
    }
    hist_put_head(out, type);
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
    fputc('\r', out);
    fclose(out);
    hist_mark_txt();
    hist_maybe_compact();
    return 0;
#else
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
    (void)off;
    fclose(out);
    return 0;
#endif
}

void hist_new(void)
{
    FILE *f;

#ifndef A2CHAT_HOST
    __asm__("cld");
#endif
    f = hist_fopen("wb");
    if (f) {
        fclose(f);
        hist_mark_txt();
    }
}

#ifndef A2CHAT_HOST
#define HIST_RAW_AUX 0x6000u
#define HIST_RAW_MAX 0x1F00u

static uint16_t hraw_len;
static uint16_t hraw_pos;

static int hgetc(void)
{
    unsigned char c;

    if (hraw_pos >= hraw_len) {
        return EOF;
    }
    aux_read((unsigned)(HIST_RAW_AUX + hraw_pos), &c, 1);
    hraw_pos++;
    return c & 0x7f;
}

static uint16_t emit_role(uint16_t off, int assistant)
{
    off = aux_add_str(off, ",{\"role\":\"");
    off = aux_add_str(off, assistant ? "assistant" : "user");
    return aux_add_str(off, "\",\"content\":\"");
}

/* Copy LOG into aux, fclose, then JSON. FILEIO $BA00 is eth_outp. */
uint16_t hist_emit_json_aux(uint16_t off)
{
    FILE *f;
    int hold;
    int c;
    uint16_t start;
    uint16_t budget;
    long sz;
    size_t nread;

    __asm__("cld");
    aux_mainbank();
    f = fopen(hist_path(), "rb");
    if (!f) {
        return off;
    }
    budget = g_cfg.maxhist ? g_cfg.maxhist : 4096u;
    if (budget > HIST_RAW_MAX) {
        budget = HIST_RAW_MAX;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    if (sz > (long)budget) {
        fseek(f, sz - (long)budget, SEEK_SET);
    } else {
        fseek(f, 0, SEEK_SET);
    }
    hraw_len = 0;
    while (hraw_len < budget &&
           (nread = fread(iobuf, 1, sizeof(iobuf), f)) > 0) {
        if ((unsigned)hraw_len + (unsigned)nread > budget) {
            nread = budget - hraw_len;
        }
        aux_write((unsigned)(HIST_RAW_AUX + hraw_len),
                  (const unsigned char *)iobuf, (unsigned)nread);
        hraw_len = (uint16_t)(hraw_len + nread);
    }
    fclose(f);
    aux_mainbank();
    hold = 0;
    hraw_pos = 0;
    start = off;
    for (;;) {
        int ai;
        unsigned n;

        if (hold) {
            c = hold;
            hold = 0;
        } else {
            c = hgetc();
        }
        if (c == EOF) {
            break;
        }
        if (c != '>') {
            continue;
        }
        ai = 0;
        c = hgetc();
        if (c == 'A') {
            ai = 1;
        }
        while (c != EOF && c != '\r' && c != '\n') {
            c = hgetc();
        }
        off = emit_role(off, ai);
        n = 0;
        for (;;) {
            if (n >= sizeof(iobuf) - 2) {
                off = json_escape_aux(off, iobuf, n);
                n = 0;
            }
            c = hgetc();
            if (c == EOF) {
                break;
            }
            if (c == '\r' || c == '\n') {
                int n2 = hgetc();
                if (n2 == '>' || n2 == EOF) {
                    hold = n2;
                    break;
                }
                iobuf[n++] = '\n';
                if (n2 != '\r' && n2 != '\n') {
                    iobuf[n++] = (char)n2;
                }
            } else {
                iobuf[n++] = (char)c;
            }
        }
        if (n) {
            off = json_escape_aux(off, iobuf, n);
        }
        off = aux_add_str(off, "\"}");
        if ((uint16_t)(off - start) >= budget) {
            break;
        }
        if (hold == EOF) {
            break;
        }
    }
    aux_mainbank();
    return off;
}
#endif

#ifdef A2CHAT_HOST
int hist_write_messages(FILE *body)
{
    FILE *f;
    long sz;
    long pos;
    long start;
    uint16_t budget;

#ifndef A2CHAT_HOST
    __asm__("cld");
#endif
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
        /* Replay user, assistant, and tool results. Skip old
         * "(streamed)" markers that had no real reply text. */
        emit = ((t == 'U' || t == 'A' || t == 'T') && pos >= start);
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
        if (t == 'A') {
            fputs("{\"role\":\"assistant\",\"content\":\"", body);
        } else if (t == 'T') {
            fputs("{\"role\":\"user\",\"content\":\"TOOL: ", body);
        } else {
            fputs("{\"role\":\"user\",\"content\":\"", body);
        }
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
#endif
