#include "a2chat.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <apple2_filetype.h>

#define iobuf g_io80

unsigned est_secs_write(unsigned bytes)
{
    unsigned s = (bytes + 11263) / 11264;
    if (s == 0 && bytes) {
        s = 1;
    }
    return s;
}

static const char *basename_path(const char *path)
{
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

/* ProDOS 8 leaf: 1–15 chars, starts with A–Z, then A–Z / 0–9 / one '.'. */
void prodos_leaf_name(char *dst, const char *in)
{
    const char *s;
    char base[16];
    char ext[4];
    unsigned bn = 0;
    unsigned en = 0;
    unsigned i;
    const char *dot = 0;

    if (in) {
        s = basename_path(in);
    } else {
        s = "FILE";
    }
    dst[0] = 0;
    for (i = 0; s[i]; i++) {
        if (s[i] == '.') {
            dot = s + i;
        }
    }
    while (*s && *s != '.') {
        char c = *s++;
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 32);
        }
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            if (bn < 15) {
                base[bn++] = c;
            }
        }
    }
    if (dot && dot[1]) {
        s = dot + 1;
        while (*s && en < 3) {
            char c = *s++;
            if (c >= 'a' && c <= 'z') {
                c = (char)(c - 32);
            }
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                ext[en++] = c;
            }
        }
    }
    if (!bn) {
        base[bn++] = 'F';
        base[bn++] = 'I';
        base[bn++] = 'L';
        base[bn++] = 'E';
    }
    if (base[0] >= '0' && base[0] <= '9') {
        if (bn > 14) {
            bn = 14;
        }
        memmove(base + 1, base, bn);
        base[0] = 'F';
        bn++;
    }
    if (en && bn + 1 + en > 15) {
        bn = (unsigned)(15 - 1 - en);
        if (!bn) {
            bn = 1;
            en = 13;
        }
    }
    memcpy(dst, base, bn);
    if (en) {
        dst[bn++] = '.';
        memcpy(dst + bn, ext, en);
        bn += en;
    }
    dst[bn] = 0;
}

static int path_has_prefix(const char *path, const char *pfx)
{
    unsigned i;

    if (!pfx || !pfx[0]) {
        return 1;
    }
    for (i = 0; pfx[i]; i++) {
        if (toupper((unsigned char)path[i]) != toupper((unsigned char)pfx[i])) {
            return 0;
        }
    }
    return path[i] == 0 || path[i] == '/';
}

int path_allowed_write(const char *path)
{
    const char *base = basename_path(path);
    if (!strcmp(base, "PRODOS") || !strcmp(base, "A2CHAT.SYSTEM") ||
        !strcmp(base, "A2CHAT.CFG") || !strcmp(base, "A2CHAT.BIN")) {
        return 0;
    }
    if (g_cfg.prefix[0] && !path_has_prefix(path, g_cfg.prefix)) {
        return 0;
    }
    return 1;
}

int path_in_workspace(const char *path)
{
    if (!path || !path[0]) {
        return 0;
    }
    if (g_cfg.prefix[0]) {
        return path_has_prefix(path, g_cfg.prefix);
    }
    return path[0] != '/' || path_has_prefix(path, ".");
}

static void path_strip_slash(char *p)
{
    unsigned n;

    if (!p || !p[0]) {
        return;
    }
    n = (unsigned)strlen(p);
    while (n > 1 && p[n - 1] == '/') {
        p[--n] = 0;
    }
}

static void path_join_ex(char *dst, const char *in, int writing)
{
    char leaf[16];
    const char *prebase;
    size_t n;

    if (!in || !in[0] || (in[0] == '.' && in[1] == 0)) {
        if (g_cfg.prefix[0]) {
            strncpy(dst, g_cfg.prefix, A2CHAT_PATH_MAX - 1);
            dst[A2CHAT_PATH_MAX - 1] = 0;
            path_strip_slash(dst);
            return;
        }
        dst[0] = '.';
        dst[1] = 0;
        return;
    }
    prodos_leaf_name(leaf, in);
    if (g_cfg.prefix[0]) {
        prebase = basename_path(g_cfg.prefix);
        if (!strcmp(leaf, prebase)) {
            if (writing) {
                strcpy(leaf, "NOTE.MD");
            } else {
                strncpy(dst, g_cfg.prefix, A2CHAT_PATH_MAX - 1);
                dst[A2CHAT_PATH_MAX - 1] = 0;
                path_strip_slash(dst);
                return;
            }
        }
        strncpy(dst, g_cfg.prefix, A2CHAT_PATH_MAX - 1);
        dst[A2CHAT_PATH_MAX - 1] = 0;
        path_strip_slash(dst);
        n = strlen(dst);
        if (n && dst[n - 1] != '/' && n + 1 < A2CHAT_PATH_MAX) {
            dst[n++] = '/';
            dst[n] = 0;
        }
        strncat(dst, leaf, A2CHAT_PATH_MAX - 1 - strlen(dst));
        return;
    }
    strncpy(dst, leaf, A2CHAT_PATH_MAX - 1);
    dst[A2CHAT_PATH_MAX - 1] = 0;
}

void path_join_prefix(char *dst, const char *in)
{
    path_join_ex(dst, in, 1);
}

void path_join_open(char *dst, const char *in)
{
    path_join_ex(dst, in, 0);
}

#ifndef A2CHAT_HOST
/* cc65 opendir() mallocs a 512-byte DIR; our heap is the few bytes
 * between BSS and FILEIO $BA00, so it always fails. Read the directory
 * file with fopen (uses the 1K FILEIO buffer) and parse entries. */
static int dir_try(char *path)
{
    FILE *f;
    unsigned pos;
    unsigned first;
    unsigned count;
    unsigned i;
    unsigned nl;
    unsigned st;

    path_strip_slash(path);
    if (!path[0] || (path[0] == '.' && path[1] == 0)) {
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    pos = 0;
    first = 1;
    count = 0;
    for (;;) {
        unsigned k = pos & 511u;

        if (k < 4) {
            if (fread(iobuf, 1, 4 - k, f) != 4 - k) {
                break;
            }
            pos += 4 - k;
            continue;
        }
        if (k == 511) {
            if (fgetc(f) == EOF) {
                break;
            }
            pos++;
            continue;
        }
        if (fread(iobuf, 1, 39, f) != 39) {
            break;
        }
        pos += 39;
        st = ((unsigned char)iobuf[0]) >> 4;
        nl = ((unsigned char)iobuf[0]) & 0x0F;
        if (first) {
            first = 0;
            if (st != 0x0E && st != 0x0F) {
                fclose(f);
                return -1;
            }
            ui_print(path);
            ui_nl();
            continue;
        }
        if (!nl || st == 0 || st == 0x0E || st == 0x0F) {
            continue;
        }
        if (nl > 15) {
            nl = 15;
        }
        for (i = 0; i < nl; i++) {
            iobuf[i] = (char)((unsigned char)iobuf[i + 1] & 0x7f);
        }
        iobuf[nl] = 0;
        ui_print(iobuf);
        ui_nl();
        if (++count >= A2CHAT_DIR_MAX) {
            break;
        }
    }
    fclose(f);
    if (first) {
        return -1;
    }
    if (!count) {
        ui_print("(empty)");
        ui_nl();
    }
    return 0;
}
#endif

int prodos_list(const char *path, char *out, unsigned outsz)
{
    char try[A2CHAT_PATH_MAX];

    (void)out;
    (void)outsz;
#ifndef A2CHAT_HOST
    if (path && path[0]) {
        strncpy(try, path, sizeof(try) - 1);
        try[sizeof(try) - 1] = 0;
        if (dir_try(try) == 0) {
            return 0;
        }
    }
    if (g_cfg.prefix[0]) {
        strncpy(try, g_cfg.prefix, sizeof(try) - 1);
        try[sizeof(try) - 1] = 0;
        if (dir_try(try) == 0) {
            return 0;
        }
    }
    if (p8_prefix(iobuf) && iobuf[0]) {
        strncpy(try, iobuf, sizeof(try) - 1);
        try[sizeof(try) - 1] = 0;
        if (dir_try(try) == 0) {
            return 0;
        }
    }
#endif
    ui_print("cannot open ");
    ui_print(path && path[0] ? (char *)path : ".");
    ui_nl();
    return -1;
}

int prodos_write_file(const char *path, const char *src_path, uint8_t ptype,
                      unsigned auxtype, int exempt)
{
    FILE *in;
    FILE *out;
    size_t n;
    unsigned total;

    total = 0;
    if (!path_allowed_write(path) && !exempt) {
        return -2;
    }
    in = fopen(src_path, "rb");
    if (!in) {
        return -1;
    }
    _filetype = ptype;
    _auxtype = auxtype;
    out = fopen(path, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    ui_status("Saving...");
    while ((n = fread(iobuf, 1, sizeof(iobuf), in)) > 0) {
        unsigned i;
        if (!exempt && total + (unsigned)n > g_cfg.maxwrite) {
            n = g_cfg.maxwrite - total;
        }
        if (ptype == PRODOS_T_TXT || ptype == PRODOS_T_BAS) {
            for (i = 0; i < (unsigned)n; i++) {
                if (iobuf[i] == '\n') {
                    iobuf[i] = '\r';
                }
            }
        }
        fwrite(iobuf, 1, n, out);
        total += (unsigned)n;
        if (!exempt && total >= g_cfg.maxwrite) {
            break;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

int prodos_write_bas(const char *path, const char *src_path, int exempt)
{
    return prodos_write_file(path, src_path, PRODOS_T_BAS, 0x0801, exempt);
}

int prodos_read_to_aux(const char *path, unsigned offset, unsigned length,
                       unsigned *got, int as_hex)
{
    FILE *f;
    unsigned n;
    unsigned done;
    unsigned want;
    unsigned au;
    static const char HEX[] = "0123456789ABCDEF";

    *got = 0;
    want = length;
    if (want > g_cfg.maxread) {
        want = g_cfg.maxread;
    }
    if (want > A2CHAT_AUX_POST_MAX) {
        want = A2CHAT_AUX_POST_MAX;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (offset) {
        fseek(f, (long)offset, SEEK_SET);
    }
    done = 0;
    au = 0;
    while (done < want) {
        unsigned chunk = want - done;
        unsigned i;

        if (chunk > sizeof(iobuf)) {
            chunk = sizeof(iobuf);
        }
        n = (unsigned)fread(iobuf, 1, chunk, f);
        if (n == 0) {
            break;
        }
        if (as_hex) {
            for (i = 0; i < n && au + 3 < want; i++) {
                unsigned char b = (unsigned char)iobuf[i];
                unsigned char hx[3];

                hx[0] = (unsigned char)HEX[b >> 4];
                hx[1] = (unsigned char)HEX[b & 15];
                hx[2] = ((i & 15) == 15) ? '\n' : ' ';
                aux_write(au, hx, 3);
                au += 3;
            }
        } else {
            for (i = 0; i < n; i++) {
                if (iobuf[i] == '\r') {
                    iobuf[i] = '\n';
                }
            }
            aux_write(au, (unsigned char *)iobuf, n);
            au += n;
        }
        done += n;
        if (n < chunk) {
            break;
        }
    }
    fclose(f);
    *got = as_hex ? au : done;
    return 0;
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)toupper((unsigned char)c);
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

int prodos_write_hex_file(const char *path, const char *hex_path, uint8_t ptype,
                          unsigned auxtype, int exempt)
{
    FILE *in;
    FILE *out;
    int hi;
    int c;
    unsigned total;

    hi = -1;
    total = 0;
    if (!path_allowed_write(path) && !exempt) {
        return -2;
    }
    in = fopen(hex_path, "rb");
    if (!in) {
        return -1;
    }
    _filetype = ptype;
    _auxtype = auxtype;
    out = fopen(path, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    ui_status("Saving...");
    while ((c = fgetc(in)) != EOF) {
        int v;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ',') {
            continue;
        }
        v = hex_nibble((char)c);
        if (v < 0) {
            continue;
        }
        if (hi < 0) {
            hi = v;
        } else {
            unsigned char b = (unsigned char)((hi << 4) | v);
            hi = -1;
            if (!exempt && total >= g_cfg.maxwrite) {
                break;
            }
            fputc(b, out);
            total++;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}
