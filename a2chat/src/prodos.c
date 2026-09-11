#include "a2chat.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <apple2_filetype.h>

static char iobuf[128];

unsigned est_secs_write(unsigned bytes)
{
    unsigned s = (bytes + 11263) / 11264;
    if (s == 0 && bytes) {
        s = 1;
    }
    return s;
}

unsigned est_secs_read(unsigned bytes)
{
    unsigned s = (bytes + 22527) / 22528;
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
    const char *use = in;
    if (!use || !use[0] || (use[0] == '.' && use[1] == 0)) {
        if (g_cfg.prefix[0]) {
            strncpy(dst, g_cfg.prefix, A2CHAT_PATH_MAX - 1);
            dst[A2CHAT_PATH_MAX - 1] = 0;
            return;
        }
        dst[0] = '.';
        dst[1] = 0;
        return;
    }
    if (g_cfg.prefix[0] && use[0] == '/' && !path_has_prefix(use, g_cfg.prefix)) {
        /* Model invented another volume; keep only the file name under PREFIX. */
        strncpy(iobuf, basename_path(use), sizeof(iobuf) - 1);
        iobuf[sizeof(iobuf) - 1] = 0;
        use = iobuf;
    } else if (use[0] == '/') {
        strncpy(dst, use, A2CHAT_PATH_MAX - 1);
        dst[A2CHAT_PATH_MAX - 1] = 0;
        return;
    }
    if (g_cfg.prefix[0]) {
        size_t n = strlen(g_cfg.prefix);
        strncpy(dst, g_cfg.prefix, A2CHAT_PATH_MAX - 1);
        dst[A2CHAT_PATH_MAX - 1] = 0;
        n = strlen(dst);
        if (n && dst[n - 1] != '/' && n + 1 < A2CHAT_PATH_MAX) {
            dst[n++] = '/';
            dst[n] = 0;
        }
        strncat(dst, use, A2CHAT_PATH_MAX - 1 - strlen(dst));
        return;
    }
    strncpy(dst, use, A2CHAT_PATH_MAX - 1);
    dst[A2CHAT_PATH_MAX - 1] = 0;
}

int prodos_list(const char *path, char *out, unsigned outsz)
{
    DIR *d;
    struct dirent *ent;
    unsigned used = 0;
    unsigned count = 0;
    static char cwd[64];
    const char *p = path;

    out[0] = 0;
    if (!p || !p[0] || (p[0] == '.' && p[1] == 0)) {
        if (g_cfg.prefix[0]) {
            p = g_cfg.prefix;
        } else if (*getcwd(cwd, sizeof(cwd))) {
            p = cwd;
        } else {
            p = ".";
        }
    }
    d = opendir((char *)p);
    if (!d) {
        strncpy(out, "cannot open ", outsz - 1);
        strncat(out, p, outsz - strlen(out) - 1);
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

int prodos_read(const char *path, unsigned offset, unsigned length,
                char *out, unsigned outsz, unsigned *got, int as_hex)
{
    FILE *f;
    unsigned n;
    unsigned i;
    unsigned want;
    unsigned done;

    *got = 0;
    out[0] = 0;
    done = 0;
    want = length;
    if (want > g_cfg.maxread) {
        want = g_cfg.maxread;
    }
    f = fopen(path, "rb");
    if (!f) {
        strncpy(out, "cannot open", outsz - 1);
        return -1;
    }
    if (offset) {
        fseek(f, (long)offset, SEEK_SET);
    }
    while (done < want) {
        unsigned chunk = want - done;
        if (chunk > sizeof(iobuf)) {
            chunk = sizeof(iobuf);
        }
        n = (unsigned)fread(iobuf, 1, chunk, f);
        if (n == 0) {
            break;
        }
        if (as_hex) {
            for (i = 0; i < n && *got + 3 < outsz; i++) {
                sprintf(out + *got, "%02X", (unsigned char)iobuf[i]);
                *got += 2;
                if ((i & 15) == 15 && *got + 1 < outsz) {
                    out[(*got)++] = '\n';
                } else if (*got + 1 < outsz) {
                    out[(*got)++] = ' ';
                }
            }
        } else {
            if (*got + n >= outsz) {
                n = outsz - *got - 1;
            }
            memcpy(out + *got, iobuf, n);
            *got += n;
            out[*got] = 0;
        }
        done += n;
        if (n < chunk) {
            break;
        }
    }
    fclose(f);
    return 0;
}

static int write_from_mem(const char *path, const char *data, unsigned len,
                          uint8_t ptype, unsigned auxtype, int exempt)
{
    FILE *f;
    unsigned w;

    if (!exempt && len > g_cfg.maxwrite) {
        len = g_cfg.maxwrite;
    }
    if (!path_allowed_write(path) && !exempt) {
        return -2;
    }
    _filetype = ptype;
    _auxtype = auxtype;
    f = fopen(path, "wb");
    if (!f) {
        return -1;
    }
    ui_status("Saving...");
    w = (unsigned)fwrite(data, 1, len, f);
    fclose(f);
    return w == len ? 0 : -1;
}

int prodos_write_text(const char *path, const char *data, unsigned len, int exempt)
{
    return write_from_mem(path, data, len, PRODOS_T_TXT, 0, exempt);
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
        if (!exempt && total + (unsigned)n > g_cfg.maxwrite) {
            n = g_cfg.maxwrite - total;
            fwrite(iobuf, 1, n, out);
            break;
        }
        fwrite(iobuf, 1, n, out);
        total += (unsigned)n;
    }
    fclose(in);
    fclose(out);
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
