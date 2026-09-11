#include "a2chat.h"
#include <stdio.h>

/* Packed tokens: length, token, name. Longest match wins. */
static const unsigned char KW[] = {
    7, 0x9E, 'I','N','V','E','R','S','E',
    7, 0x9C, 'N','O','T','R','A','C','E',
    7, 0xAE, 'R','E','S','T','O','R','E',
    7, 0x92, 'H','C','O','L','O','R','=',
    6, 0xE9, 'R','I','G','H','T','$',
    6, 0x9A, 'S','H','L','O','A','D',
    6, 0xA3, 'H','I','M','E','M',':',
    6, 0xA4, 'L','O','M','E','M',':',
    6, 0xA9, 'S','P','E','E','D','=',
    6, 0xA0, 'C','O','L','O','R','=',
    6, 0x99, 'S','C','A','L','E','=',
    6, 0xA6, 'R','E','S','U','M','E',
    6, 0xA7, 'R','E','C','A','L','L',
    6, 0xB1, 'R','E','T','U','R','N',
    6, 0x9D, 'N','O','R','M','A','L',
    5, 0xA5, 'O','N','E','R','R',
    5, 0x93, 'H','P','L','O','T',
    5, 0x95, 'X','D','R','A','W',
    5, 0x9B, 'T','R','A','C','E',
    5, 0x9F, 'F','L','A','S','H',
    5, 0xB0, 'G','O','S','U','B',
    5, 0xBA, 'P','R','I','N','T',
    5, 0xBD, 'C','L','E','A','R',
    5, 0x84, 'I','N','P','U','T',
    5, 0xA8, 'S','T','O','R','E',
    5, 0xE8, 'L','E','F','T','$',
    5, 0xD7, 'S','C','R','N','(',
    4, 0x83, 'D','A','T','A',
    4, 0x87, 'R','E','A','D',
    4, 0x90, 'H','G','R','2',
    4, 0x8E, 'H','L','I','N',
    4, 0x8F, 'V','L','I','N',
    4, 0x94, 'D','R','A','W',
    4, 0x96, 'H','T','A','B',
    4, 0x97, 'H','O','M','E',
    4, 0x98, 'R','O','T','=',
    4, 0xA2, 'V','T','A','B',
    4, 0xAB, 'G','O','T','O',
    4, 0xB3, 'S','T','O','P',
    4, 0xB5, 'W','A','I','T',
    4, 0xB6, 'L','O','A','D',
    4, 0xB7, 'S','A','V','E',
    4, 0xB9, 'P','O','K','E',
    4, 0xBB, 'C','O','N','T',
    4, 0xBC, 'L','I','S','T',
    4, 0xC0, 'T','A','B','(',
    4, 0xC3, 'S','P','C','(',
    4, 0xC4, 'T','H','E','N',
    4, 0xC7, 'S','T','E','P',
    4, 0xE4, 'S','T','R','$',
    4, 0xE7, 'C','H','R','$',
    4, 0xEA, 'M','I','D','$',
    4, 0xE2, 'P','E','E','K',
    4, 0x82, 'N','E','X','T',
    4, 0x89, 'T','E','X','T',
    4, 0x8C, 'C','A','L','L',
    4, 0x8D, 'P','L','O','T',
    3, 0x91, 'H','G','R',
    3, 0x8A, 'P','R','#',
    3, 0x8B, 'I','N','#',
    3, 0xA1, 'P','O','P',
    3, 0xAC, 'R','U','N',
    3, 0xB2, 'R','E','M',
    3, 0xBF, 'N','E','W',
    3, 0xC6, 'N','O','T',
    3, 0xCD, 'A','N','D',
    3, 0xD2, 'S','G','N',
    3, 0xD3, 'I','N','T',
    3, 0xD4, 'A','B','S',
    3, 0xD5, 'U','S','R',
    3, 0xD6, 'F','R','E',
    3, 0xD8, 'P','D','L',
    3, 0xD9, 'P','O','S',
    3, 0xDA, 'S','Q','R',
    3, 0xDB, 'R','N','D',
    3, 0xDC, 'L','O','G',
    3, 0xDD, 'E','X','P',
    3, 0xDE, 'C','O','S',
    3, 0xDF, 'S','I','N',
    3, 0xE0, 'T','A','N',
    3, 0xE3, 'L','E','N',
    3, 0xE5, 'V','A','L',
    3, 0xE6, 'A','S','C',
    3, 0xE1, 'A','T','N',
    3, 0x80, 'E','N','D',
    3, 0x81, 'F','O','R',
    3, 0x85, 'D','E','L',
    3, 0x86, 'D','I','M',
    3, 0xAA, 'L','E','T',
    3, 0xB8, 'D','E','F',
    3, 0xBE, 'G','E','T',
    2, 0x88, 'G','R',
    2, 0xB4, 'O','N',
    2, 0xC1, 'T','O',
    2, 0xAD, 'I','F',
    2, 0xC2, 'F','N',
    2, 0xC5, 'A','T',
    2, 0xCE, 'O','R',
    1, 0xAF, '&',
    1, 0xC8, '+',
    1, 0xC9, '-',
    1, 0xCA, '*',
    1, 0xCB, '/',
    1, 0xCC, '^',
    0
};

static char upch(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

static int ident(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '$';
}

static int match_kw(const char *p, unsigned *n, unsigned char *tok)
{
    const unsigned char *k = KW;
    unsigned best_n = 0;
    unsigned char best_t = 0;

    while (k[0]) {
        unsigned len = k[0];
        unsigned i;
        int ok = 1;
        for (i = 0; i < len; i++) {
            if (upch(p[i]) != upch((char)k[2 + i])) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            char last = (char)k[1 + len];
            if (((last >= 'A' && last <= 'Z') || last == '$') && ident(p[len])) {
                ok = 0;
            }
        }
        if (ok && len > best_n) {
            best_n = len;
            best_t = k[1];
        }
        k += 2 + len;
    }
    if (best_n) {
        *n = best_n;
        *tok = best_t;
        return 1;
    }
    if (p[0] == '>' || p[0] == '=' || p[0] == '<') {
        *n = 1;
        *tok = (p[0] == '>') ? 0xCF : (p[0] == '=') ? 0xD0 : 0xD1;
        return 1;
    }
    return 0;
}

int bas_header_is_tokenized(const unsigned char *b, unsigned n)
{
    unsigned next, line;
    if (n < 5 || (b[0] >= '0' && b[0] <= '9')) {
        return 0;
    }
    next = (unsigned)b[0] | ((unsigned)b[1] << 8);
    line = (unsigned)b[2] | ((unsigned)b[3] << 8);
    return next >= 0x0801u && next <= 0x9600u && line <= 63999u;
}

static int held = -2;

static int bgetc(FILE *in)
{
    int c;
    if (held != -2) {
        c = held;
        held = -2;
        return c;
    }
    return fgetc(in);
}

static int readline2(FILE *in, char *line, unsigned max)
{
    unsigned li = 0;
    int c;

    for (;;) {
        c = bgetc(in);
        if (c == EOF) {
            break;
        }
        if (c == '\r' || c == '\n') {
            if (c == '\r') {
                c = bgetc(in);
                if (c != '\n' && c != EOF) {
                    held = c;
                }
            }
            break;
        }
        if (li + 1 < max) {
            line[li++] = (char)c;
        }
    }
    line[li] = 0;
    if (c == EOF && li == 0) {
        return -1;
    }
    return 0;
}

static char line[128];
static unsigned char tok[128];

int bas_tokenize(FILE *in, FILE *out)
{
    unsigned addr = 0x0801;
    unsigned lines = 0;

    held = -2;
    while (readline2(in, line, sizeof(line)) == 0) {
        unsigned n = 0, lnum = 0, tn = 0, m;
        unsigned char t;
        uint8_t in_str = 0, rem = 0;
        unsigned next;

        while (line[n] == ' ' || line[n] == '\t') {
            n++;
        }
        if (!line[n] || line[n] < '0' || line[n] > '9') {
            continue;
        }
        while (line[n] >= '0' && line[n] <= '9') {
            lnum = lnum * 10 + (unsigned)(line[n] - '0');
            n++;
        }
        if (lnum > 63999u) {
            return -1;
        }
        while (line[n] == ' ' || line[n] == '\t') {
            n++;
        }
        while (line[n] && tn < sizeof(tok) - 1) {
            if (rem || in_str) {
                if (line[n] == '"' && in_str && !rem) {
                    in_str = 0;
                }
                tok[tn++] = (unsigned char)line[n++];
                continue;
            }
            if (line[n] == '"') {
                in_str = 1;
                tok[tn++] = '"';
                n++;
                continue;
            }
            if (line[n] == '?') {
                tok[tn++] = 0xBA;
                n++;
                continue;
            }
            if (match_kw(line + n, &m, &t)) {
                tok[tn++] = t;
                n += m;
                if (t == 0xB2 || t == 0x83) {
                    rem = 1;
                }
                continue;
            }
            tok[tn++] = (unsigned char)line[n++];
        }
        next = addr + 4 + tn + 1;
        fputc((unsigned char)(next & 0xff), out);
        fputc((unsigned char)(next >> 8), out);
        fputc((unsigned char)(lnum & 0xff), out);
        fputc((unsigned char)(lnum >> 8), out);
        fwrite(tok, 1, tn, out);
        fputc(0, out);
        addr = next;
        lines++;
    }
    fputc(0, out);
    fputc(0, out);
    return lines ? 0 : -1;
}

static const unsigned char *kw_for_tok(unsigned char t)
{
    const unsigned char *k = KW;
    while (k[0]) {
        if (k[1] == t) {
            return k;
        }
        k += 2 + k[0];
    }
    return 0;
}

static FILE *g_out;
static unsigned g_got, g_max;
#ifndef A2CHAT_HOST
static unsigned char g_chunk[16];
static unsigned g_cn;
static uint8_t g_aux;
#endif

static int dput(char ch)
{
#ifndef A2CHAT_HOST
    if (g_aux) {
        if (g_got >= g_max) {
            return -1;
        }
        g_chunk[g_cn++] = (unsigned char)ch;
        g_got++;
        if (g_cn >= 16) {
            aux_write(g_got - g_cn, g_chunk, g_cn);
            g_cn = 0;
        }
        return 0;
    }
#endif
    if (g_out) {
        fputc((unsigned char)ch, g_out);
        g_got++;
    }
    return 0;
}

static void dflush(void)
{
#ifndef A2CHAT_HOST
    if (g_aux && g_cn) {
        aux_write(g_got - g_cn, g_chunk, g_cn);
        g_cn = 0;
    }
#endif
}

static int detok_stream(FILE *in)
{
    g_got = 0;
#ifndef A2CHAT_HOST
    g_cn = 0;
#endif
    for (;;) {
        int b0 = fgetc(in);
        int b1;
        unsigned next, line, v;
        const unsigned char *k;
        unsigned i;
        char tmp[6];
        unsigned ti;

        if (b0 == EOF) {
            break;
        }
        b1 = fgetc(in);
        if (b1 == EOF) {
            break;
        }
        next = (unsigned)(b0 & 0xff) | ((unsigned)(b1 & 0xff) << 8);
        if (next == 0) {
            break;
        }
        b0 = fgetc(in);
        b1 = fgetc(in);
        if (b0 == EOF || b1 == EOF) {
            break;
        }
        line = (unsigned)(b0 & 0xff) | ((unsigned)(b1 & 0xff) << 8);
        v = line;
        ti = 0;
        if (v == 0) {
            tmp[ti++] = '0';
        } else {
            while (v && ti < 5) {
                tmp[ti++] = (char)('0' + (v % 10));
                v /= 10;
            }
        }
        while (ti) {
            if (dput(tmp[--ti]) < 0) {
                dflush();
                return 0;
            }
        }
        if (dput(' ') < 0) {
            dflush();
            return 0;
        }
        for (;;) {
            int ch = fgetc(in);
            if (ch == EOF || ch == 0) {
                break;
            }
            if ((unsigned char)ch < 0x80) {
                if (dput((char)ch) < 0) {
                    dflush();
                    return 0;
                }
                continue;
            }
            k = kw_for_tok((unsigned char)ch);
            if (k) {
                for (i = 0; i < k[0]; i++) {
                    if (dput((char)k[2 + i]) < 0) {
                        dflush();
                        return 0;
                    }
                }
            }
        }
        if (dput('\n') < 0) {
            dflush();
            return 0;
        }
    }
    dflush();
    return 0;
}

int bas_detokenize(FILE *in, FILE *out)
{
    g_out = out;
    g_max = 0xFFFFu;
#ifndef A2CHAT_HOST
    g_aux = 0;
#endif
    return detok_stream(in);
}

#ifndef A2CHAT_HOST
int bas_detokenize_aux(FILE *in, unsigned max, unsigned *got)
{
    g_out = 0;
    g_max = max;
    g_aux = 1;
    detok_stream(in);
    *got = g_got;
    return 0;
}
#endif
