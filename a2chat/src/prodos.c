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

/* ProDOS pathname, not just the leaf. Absolute paths stay absolute.
 * A relative path is joined onto PREFIX when PREFIX is set, otherwise
 * it is left for the current ProDOS prefix. Empty input yields "". */
static void path_join_ex(char *dst, const char *in, int writing)
{
    static char norm[A2CHAT_PATH_MAX];
    unsigned n = 0;
    unsigned seg = 0;
    int dotted = 0;
    const char *s = in;

    (void)writing;
    dst[0] = 0;
    if (!s || !s[0] || (s[0] == '.' && s[1] == 0)) {
        if (g_cfg.prefix[0]) {
            strncpy(dst, g_cfg.prefix, A2CHAT_PATH_MAX - 1);
            dst[A2CHAT_PATH_MAX - 1] = 0;
            path_strip_slash(dst);
        }
        return;
    }
    while (*s && n + 1 < A2CHAT_PATH_MAX) {
        unsigned char c = (unsigned char)*s++;

        if (c == '/') {
            if (n == 0 || norm[n - 1] != '/') {
                norm[n++] = '/';
                seg = 0;
                dotted = 0;
            }
            continue;
        }
        if (c >= 'a' && c <= 'z') {
            c = (unsigned char)(c - 32);
        }
        if (c == '.') {
            if (dotted || seg == 0 || seg >= 15) {
                continue;
            }
            norm[n++] = '.';
            seg++;
            dotted = 1;
            continue;
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
            continue;
        }
        if (seg >= 15 || (seg == 0 && c >= '0' && c <= '9')) {
            continue;
        }
        norm[n++] = (char)c;
        seg++;
    }
    norm[n] = 0;
    path_strip_slash(norm);
    if (!norm[0]) {
        return;
    }
    if (norm[0] != '/' && g_cfg.prefix[0]) {
        size_t pn;

        strncpy(dst, g_cfg.prefix, A2CHAT_PATH_MAX - 1);
        dst[A2CHAT_PATH_MAX - 1] = 0;
        path_strip_slash(dst);
        pn = strlen(dst);
        if (pn && dst[pn - 1] != '/' && pn + 1 < A2CHAT_PATH_MAX) {
            dst[pn++] = '/';
            dst[pn] = 0;
        }
        strncat(dst, norm, A2CHAT_PATH_MAX - 1 - strlen(dst));
        return;
    }
    strncpy(dst, norm, A2CHAT_PATH_MAX - 1);
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
extern unsigned char dirblk[];

/* cc65 opendir() mallocs a 512-byte DIR; our heap is the few bytes
 * between BSS and FILEIO $BA00, so it always fails. Read the directory
 * file with fopen (uses the 1K FILEIO buffer) and parse entries. */
static int dir_try(char *path)
{
    FILE *f;
    unsigned first;
    unsigned count;
    unsigned i;
    unsigned nl;
    unsigned st;
    unsigned ent;

    path_strip_slash(path);
    if (!path[0] || (path[0] == '.' && path[1] == 0)) {
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    first = 1;
    count = 0;
    /* ProDOS accepts a directory READ only when the count is 512. */
    while (fread(dirblk, 1, 512, f) == 512) {
        for (ent = 0; ent < 13; ent++) {
            unsigned char *e = dirblk + 4 + ent * 39;
            unsigned ft;
            const char *tag;
            char name[16];

            st = e[0] >> 4;
            nl = e[0] & 0x0F;
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
            ft = e[0x10];
            tag = "";
            if (ft == 0x04) {
                tag = " TXT";
            } else if (ft == 0x06) {
                tag = " BIN";
            } else if (ft == 0xFC) {
                tag = " BAS";
            } else if (ft == 0xFF) {
                tag = " SYS";
            } else if (ft == 0x0F) {
                tag = " DIR";
            }
            for (i = 0; i < nl; i++) {
                name[i] = (char)(e[i + 1] & 0x7f);
            }
            name[nl] = 0;
            ui_print(name);
            ui_print(tag);
            ui_nl();
            if (++count >= A2CHAT_DIR_MAX) {
                break;
            }
        }
        if (count >= A2CHAT_DIR_MAX) {
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
    /* An explicit path is only that directory. Falling through would
     * list the launch folder and hide a bad path. */
    if (path && path[0]) {
        strncpy(try, path, sizeof(try) - 1);
        try[sizeof(try) - 1] = 0;
        if (dir_try(try) == 0) {
            return 0;
        }
        ui_print("cannot open ");
        ui_print(try);
        ui_nl();
        return -1;
    }
    if (g_cfg.prefix[0]) {
        strncpy(try, g_cfg.prefix, sizeof(try) - 1);
        try[sizeof(try) - 1] = 0;
        if (dir_try(try) == 0) {
            return 0;
        }
    } else if (p8_prefix(iobuf) && iobuf[0]) {
        strncpy(try, iobuf, sizeof(try) - 1);
        try[sizeof(try) - 1] = 0;
        if (dir_try(try) == 0) {
            return 0;
        }
    }
#endif
    ui_print("cannot open .");
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
