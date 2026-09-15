#include "a2chat.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
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

void path_join_prefix(char *dst, const char *in)
{
    char leaf[16];
    const char *prebase;
    size_t n;

    if (!in || !in[0] || (in[0] == '.' && in[1] == 0)) {
        if (g_cfg.prefix[0]) {
            strncpy(dst, g_cfg.prefix, A2CHAT_PATH_MAX - 1);
            dst[A2CHAT_PATH_MAX - 1] = 0;
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
            strcpy(leaf, "NOTE.MD");
        }
        strncpy(dst, g_cfg.prefix, A2CHAT_PATH_MAX - 1);
        dst[A2CHAT_PATH_MAX - 1] = 0;
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

int prodos_list(const char *path, char *out, unsigned outsz)
{
    DIR *d;
    struct dirent *ent;
    unsigned used = 0;
    unsigned count = 0;
    const char *p = path;

    out[0] = 0;
    if (!p || !p[0] || (p[0] == '.' && p[1] == 0) ||
        (g_cfg.prefix[0] && path_has_prefix(p, g_cfg.prefix) &&
         p[strlen(g_cfg.prefix)] == 0)) {
        d = opendir(".");
        if (!d && g_cfg.prefix[0]) {
            d = opendir((char *)g_cfg.prefix);
        }
        if (!d) {
            p = ".";
        } else {
            p = 0;
        }
    } else {
        d = opendir((char *)p);
    }
    if (!d) {
        strncpy(out, "cannot open ", outsz - 1);
        strncat(out, p ? (char *)p : ".", outsz - strlen(out) - 1);
        return -1;
    }
    while ((ent = readdir(d)) != NULL && count < A2CHAT_DIR_MAX) {
        unsigned n;
        sprintf(iobuf, "%s %02X %lu\n", ent->d_name, (unsigned)ent->d_type,
                (unsigned long)ent->d_size);
        n = (unsigned)strlen(iobuf);
        if (used + n + 1 >= outsz) {
            break;
        }
        memcpy(out + used, iobuf, n + 1);
        used += n;
        count++;
    }
    closedir(d);
    if (count == A2CHAT_DIR_MAX) {
        strncat(out, "(truncated)\n", outsz - strlen(out) - 1);
    }
    return 0;
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
