#include "a2chat.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* The Apple build parses replies with jsonscan_feed_buf(), which matches
 * keys with counters and calls jsonscan_feed() only inside reply text. The
 * byte-at-a-time key window, tool calls and file payloads below are used by
 * the host build and its tests only. */
#ifdef A2CHAT_HOST
static int win_eq(const struct jsonscan *j, const char *suf, uint8_t n)
{
    unsigned i;
    unsigned p;

    if (j->winlen < n) {
        return 0;
    }
    p = j->winpos;
    i = n;
    while (i) {
        i--;
        p = (unsigned)((p + (A2CHAT_WIN - 1)) & (A2CHAT_WIN - 1));
        if (j->win[p] != suf[i]) {
            return 0;
        }
    }
    return 1;
}

static int win_ends(const struct jsonscan *j, const char *suf)
{
    return win_eq(j, suf, (uint8_t)strlen(suf));
}

static void win_add(struct jsonscan *j, char ch)
{
    j->win[j->winpos] = ch;
    j->winpos = (uint8_t)((j->winpos + 1) & (A2CHAT_WIN - 1));
    if (j->winlen < A2CHAT_WIN) {
        j->winlen++;
    }
}
#endif

static void emit_content(struct jsonscan *j, char ch)
{
    if (j->on_content) {
        j->on_content(ch, j->user);
    }
}

/* Latin-1 letters U+00C0..U+00FF without their accents. */
static const char latin1[] =
    "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";

/* Reply text is shown and saved as 7-bit ASCII. Emit one code point as its
 * nearest ASCII: curly quotes, dashes, ellipsis, odd spaces and accented
 * Latin-1 letters map over; anything else becomes '?'. */
static void emit_cp(struct jsonscan *j, uint16_t cp)
{
    char c = '?';
    uint8_t lo = (uint8_t)cp;

    if (cp < 0x80) {
        c = (char)cp;
    } else if (cp >= 0xC0 && cp <= 0xFF) {
        c = latin1[lo - 0xC0];
    } else if (cp == 0xA0) {
        c = ' ';
    } else if (cp == 0xAB || cp == 0xBB) {
        c = '"';
    } else if ((cp >> 8) == 0x20) {
        if (lo <= 0x0A || lo == 0x2F) {
            c = ' ';
        } else if (lo <= 0x0F) {
            return;                     /* zero-width marks */
        } else if (lo <= 0x15) {
            c = '-';
        } else if (lo >= 0x18 && lo <= 0x1B) {
            c = '\'';
        } else if (lo >= 0x1C && lo <= 0x1F) {
            c = '"';
        } else if (lo == 0x22) {
            c = '*';
        } else if (lo == 0x26) {
            emit_content(j, '.');
            emit_content(j, '.');
            c = '.';
        }
    } else if (cp == 0x2212) {
        c = '-';
    } else if (cp == 0xFEFF || (cp & 0xFC00) == 0xDC00) {
        return;     /* byte-order mark; low half of a \u surrogate pair */
    }
    emit_content(j, c);
}

/* One raw byte >= $80 of a UTF-8 sequence in reply text. */
static void utf8_byte(struct jsonscan *j, uint8_t b)
{
    if (b < 0xC0) {
        if (!j->u8_need) {
            emit_content(j, '?');
            return;
        }
        j->u8_cp = (uint16_t)((j->u8_cp << 6) | (b & 0x3F));
        if (--j->u8_need == 0) {
            emit_cp(j, j->u8_big ? 0xFFFFu : j->u8_cp);
        }
        return;
    }
    if (j->u8_need) {
        emit_content(j, '?');           /* previous sequence cut short */
    }
    j->u8_big = 0;
    if (b < 0xE0) {
        j->u8_need = 1;
        j->u8_cp = b & 0x1F;
    } else if (b < 0xF0) {
        j->u8_need = 2;
        j->u8_cp = b & 0x0F;
    } else {
        j->u8_need = 3;
        j->u8_big = 1;
    }
}

#ifdef A2CHAT_HOST
static void pay_ch(struct jsonscan *j, char ch)
{
    if (j->pay && j->pay_bytes < 65535u) {
        fputc((unsigned char)ch, j->pay);
        j->pay_bytes++;
    }
}
#endif

static char unescape(struct jsonscan *j, char ch)
{
    switch (ch) {
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case '"': return '"';
    case '\\': return '\\';
    case '/': return '/';
    case 'u':
        j->unicode_n = 4;
        j->unicode = 0;
        return 0;
    default:
        return ch;
    }
}

static void end_num(struct jsonscan *j)
{
    unsigned v = 0;
    uint8_t i;
    j->numbuf[j->numlen] = 0;
    for (i = 0; i < j->numlen; i++) {
        if (j->numbuf[i] >= '0' && j->numbuf[i] <= '9') {
            v = v * 10 + (unsigned)(j->numbuf[i] - '0');
        }
    }
    if (j->mode == JS_NUM_OFFSET) {
        j->arg_offset = v;
    } else if (j->mode == JS_NUM_LENGTH) {
        j->arg_length = v;
    } else if (j->mode == JS_NUM_AUX) {
        j->arg_auxtype = v;
    } else if (j->mode == JS_NUM_EVAL) {
        j->eval_count = (uint16_t)v;
    } else if (j->mode == JS_NUM_PROMPT_EVAL) {
        j->prompt_eval_count = (uint16_t)v;
    }
    j->numlen = 0;
    j->mode = JS_SEEK;
}

void jsonscan_feed(struct jsonscan *j, char ch)
{
#ifdef A2CHAT_HOST
    if (j->mode == JS_NUM_OFFSET || j->mode == JS_NUM_LENGTH ||
        j->mode == JS_NUM_AUX || j->mode == JS_NUM_EVAL ||
        j->mode == JS_NUM_PROMPT_EVAL) {
        if (ch == ' ' || ch == '\t') {
            return;
        }
        if (ch >= '0' && ch <= '9' && j->numlen < 7) {
            j->numbuf[j->numlen++] = ch;
            return;
        }
        end_num(j);
    }

    if (j->mode == JS_MSG_CONTENT || j->mode == JS_TOOL_NAME ||
        j->mode == JS_ARG_PATH || j->mode == JS_ARG_TYPE ||
        j->mode == JS_ARG_CONTENT || j->mode == JS_ARG_HEX) {
#else
    if (j->mode == JS_MSG_CONTENT) {
#endif
        if (j->unicode_n) {
            char h = (char)tolower((unsigned char)ch);
            j->unicode <<= 4;
            if (h >= '0' && h <= '9') {
                j->unicode |= (uint16_t)(h - '0');
            } else if (h >= 'a' && h <= 'f') {
                j->unicode |= (uint16_t)(h - 'a' + 10);
            }
            j->unicode_n--;
            if (j->unicode_n == 0) {
                if (j->mode == JS_MSG_CONTENT) {
                    emit_cp(j, j->unicode);
                }
#ifdef A2CHAT_HOST
                else if (j->mode == JS_ARG_CONTENT || j->mode == JS_ARG_HEX) {
                    pay_ch(j, (j->unicode < 128) ? (char)j->unicode : '?');
                }
#endif
            }
            return;
        }
        if (j->mode == JS_MSG_CONTENT && !j->escape) {
            if ((uint8_t)ch & 0x80) {
                utf8_byte(j, (uint8_t)ch);
                return;
            }
            if (j->u8_need) {
                j->u8_need = 0;
                emit_content(j, '?');   /* sequence cut short */
            }
        }
        if (j->escape) {
            char out = unescape(j, ch);
            j->escape = 0;
            if (j->unicode_n) {
                return;
            }
            ch = out;
        } else if (ch == '\\') {
            j->escape = 1;
            return;
        } else if (ch == '"') {
            j->mode = JS_SEEK;
            j->winlen = 0;
            j->winpos = 0;
            return;
        }

        if (j->mode == JS_MSG_CONTENT) {
            emit_content(j, ch);
        }
#ifdef A2CHAT_HOST
        else if (j->mode == JS_TOOL_NAME) {
            if (j->tool_nlen < A2CHAT_TOOL_NAME_MAX - 1) {
                j->tool_name[j->tool_nlen++] = ch;
                j->tool_name[j->tool_nlen] = 0;
            }
            j->has_tool = 1;
        } else if (j->mode == JS_ARG_PATH) {
            if (j->path_nlen < A2CHAT_PATH_MAX - 1) {
                j->arg_path[j->path_nlen++] = ch;
                j->arg_path[j->path_nlen] = 0;
            }
        } else if (j->mode == JS_ARG_TYPE) {
            if (j->type_nlen < 7) {
                j->arg_type[j->type_nlen++] = ch;
                j->arg_type[j->type_nlen] = 0;
            }
        } else if (j->mode == JS_ARG_CONTENT || j->mode == JS_ARG_HEX) {
            pay_ch(j, ch);
        }
#endif
        return;
    }

#ifdef A2CHAT_HOST
    win_add(j, ch);

    if (ch == ':') {
        if (win_eq(j, "_eval_count\":", 13)) {
            j->mode = JS_NUM_PROMPT_EVAL;
            j->numlen = 0;
            return;
        }
        if (win_eq(j, "\"eval_count\":", 13)) {
            j->mode = JS_NUM_EVAL;
            j->numlen = 0;
            return;
        }
    } else if (ch == 'e') {
        if (win_eq(j, "\"done\":true", 11) || win_eq(j, "\"done\": true", 12)) {
            j->done = 1;
        }
    } else if (ch == '"') {
        if (win_eq(j, "\"content\":\"", 11)) {
            j->mode = j->seen_arguments ? JS_ARG_CONTENT : JS_MSG_CONTENT;
            j->escape = 0;
            return;
        }
    }
    if (win_ends(j, "\"arguments\"")) {
        j->seen_arguments = 1;
    }
    if (win_ends(j, "tool_calls")) {
        j->seen_tool_calls = 1;
    }
    if (win_ends(j, "\"name\":\"")) {
        if (!j->seen_tool_calls) {
            return;
        }
        j->mode = JS_TOOL_NAME;
        j->tool_nlen = 0;
        j->tool_name[0] = 0;
        j->has_tool = 1;
        return;
    }
    if (win_ends(j, "\"path\":\"")) {
        j->mode = JS_ARG_PATH;
        j->path_nlen = 0;
        j->arg_path[0] = 0;
        return;
    }
    if (win_ends(j, "\"type\":\"")) {
        j->mode = JS_ARG_TYPE;
        j->type_nlen = 0;
        j->arg_type[0] = 0;
        return;
    }
    if (win_ends(j, "\"hex\":\"") || win_ends(j, "\"data\":\"")) {
        j->mode = JS_ARG_HEX;
        return;
    }
    if (win_ends(j, "\"offset\":")) {
        j->mode = JS_NUM_OFFSET;
        j->numlen = 0;
        return;
    }
    if (win_ends(j, "\"length\":")) {
        j->mode = JS_NUM_LENGTH;
        j->numlen = 0;
        return;
    }
    if (win_ends(j, "\"auxtype\":")) {
        j->mode = JS_NUM_AUX;
        j->numlen = 0;
        return;
    }
#endif
}

/* Match position across receive windows. Reset in jsonscan_init. */
static uint8_t keypos;
static uint8_t evalpos;
static uint8_t comppos;
static uint8_t predpos;

static const char key_content[] = "\"content\":\"";
static const char key_eval[] = "\"eval_count\":";
static const char key_comp[] = "\"completion_tokens\":";
/* llama.cpp puts this in timings even when usage is omitted. */
static const char key_pred[] = "\"predicted_n\":";

void jsonscan_feed_buf(struct jsonscan *j, const char *p, unsigned n)
{
    unsigned i = 0;

    while (i < n) {
        /* Ordinary JSON bytes are not letters on the screen. Match the
         * content key with a counter instead of the per-byte C scanner. */
        if (j->mode == JS_SEEK) {
            char c = p[i++];

            if (c == key_content[keypos]) {
                keypos++;
                if (keypos == 11) {
                    j->mode = j->seen_arguments ? JS_ARG_CONTENT : JS_MSG_CONTENT;
                    j->escape = 0;
                    keypos = 0;
                }
            } else {
                keypos = (c == '"') ? 1 : 0;
            }
            if (c == key_eval[evalpos]) {
                evalpos++;
                if (evalpos == 13) {
                    j->mode = JS_NUM_EVAL;
                    j->numlen = 0;
                    evalpos = 0;
                    comppos = 0;
                    predpos = 0;
                }
            } else {
                evalpos = (c == '"') ? 1 : 0;
            }
            if (j->mode == JS_SEEK) {
                if (c == key_comp[comppos]) {
                    comppos++;
                    if (comppos == sizeof(key_comp) - 1) {
                        j->mode = JS_NUM_EVAL;
                        j->numlen = 0;
                        comppos = 0;
                        evalpos = 0;
                        predpos = 0;
                    }
                } else {
                    comppos = (c == '"') ? 1 : 0;
                }
            }
            if (j->mode == JS_SEEK) {
                if (c == key_pred[predpos]) {
                    predpos++;
                    if (predpos == sizeof(key_pred) - 1) {
                        j->mode = JS_NUM_EVAL;
                        j->numlen = 0;
                        predpos = 0;
                        evalpos = 0;
                        comppos = 0;
                    }
                } else {
                    predpos = (c == '"') ? 1 : 0;
                }
            }
            continue;
        }
        if (j->mode == JS_NUM_EVAL) {
            char c = p[i];

            if (c == ' ') {
                i++;
                continue;
            }
            if (c >= '0' && c <= '9' && j->numlen < 7) {
                j->numbuf[j->numlen++] = c;
                i++;
                continue;
            }
            end_num(j);
            continue;
        }
        if (j->mode == JS_MSG_CONTENT && !j->escape && !j->unicode_n &&
            !j->u8_need) {
            unsigned k = 0;
            const char *q = p + i;

            while (i + k < n) {
                char c = q[k];
                if (c == '"' || c == '\\' || ((uint8_t)c & 0x80)) {
                    break;
                }
                k++;
            }
            if (k) {
                if (j->on_span) {
                    j->on_span(q, k, j->user);
                } else {
                    unsigned t;
                    for (t = 0; t < k; t++) {
                        emit_content(j, q[t]);
                    }
                }
                i += k;
                continue;
            }
        }
        jsonscan_feed(j, p[i]);
        i++;
    }
}

void jsonscan_init(struct jsonscan *j)
{
    memset(j, 0, sizeof(*j));
    keypos = 0;
    evalpos = 0;
    comppos = 0;
    predpos = 0;
}

#ifdef A2CHAT_HOST
void jsonscan_on_byte(char ch, void *user)
{
    jsonscan_feed((struct jsonscan *)user, ch);
}
#endif

void jsonscan_on_bytes(const char *p, unsigned n, void *user)
{
    jsonscan_feed_buf((struct jsonscan *)user, p, n);
}

#ifndef A2CHAT_HOST
uint16_t aux_add_str(uint16_t off, const char *s)
{
    unsigned n;

    n = (unsigned)strlen(s);
    if (off >= A2CHAT_AUX_POST_MAX) {
        return off;
    }
    if ((unsigned)off + n > A2CHAT_AUX_POST_MAX) {
        n = A2CHAT_AUX_POST_MAX - (unsigned)off;
    }
    if (n) {
        aux_write(off, (const unsigned char *)s, n);
    }
    return (uint16_t)(off + n);
}

uint16_t json_escape_aux(uint16_t off, const char *s, unsigned n)
{
    unsigned i;
    char pair[2];

    for (i = 0; i < n && off < A2CHAT_AUX_POST_MAX; i++) {
        unsigned char c = (unsigned char)s[i] & 0x7f;
        if (c == 0) {
            continue;
        }
        if (c == '"' || c == '\\') {
            pair[0] = '\\';
            pair[1] = (char)c;
            if ((unsigned)off + 2 > A2CHAT_AUX_POST_MAX) {
                break;
            }
            aux_write(off, (const unsigned char *)pair, 2);
            off = (uint16_t)(off + 2);
        } else if (c == '\n') {
            pair[0] = '\\';
            pair[1] = 'n';
            if ((unsigned)off + 2 > A2CHAT_AUX_POST_MAX) {
                break;
            }
            aux_write(off, (const unsigned char *)pair, 2);
            off = (uint16_t)(off + 2);
        } else if (c == '\r') {
            pair[0] = '\\';
            pair[1] = 'r';
            if ((unsigned)off + 2 > A2CHAT_AUX_POST_MAX) {
                break;
            }
            aux_write(off, (const unsigned char *)pair, 2);
            off = (uint16_t)(off + 2);
        } else if (c == '\t') {
            pair[0] = '\\';
            pair[1] = 't';
            if ((unsigned)off + 2 > A2CHAT_AUX_POST_MAX) {
                break;
            }
            aux_write(off, (const unsigned char *)pair, 2);
            off = (uint16_t)(off + 2);
        } else if (c >= 32) {
            aux_write(off, &c, 1);
            off = (uint16_t)(off + 1);
        }
    }
    return off;
}
#endif

void json_escape_fwrite(FILE *f, const char *s, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i] & 0x7f;
        if (c == 0) {
            continue;
        }
        if (c == '"' || c == '\\') {
            fputc('\\', f);
            fputc(c, f);
        } else if (c == '\n') {
            fputc('\\', f);
            fputc('n', f);
        } else if (c == '\r') {
            fputc('\\', f);
            fputc('r', f);
        } else if (c == '\t') {
            fputc('\\', f);
            fputc('t', f);
        } else if (c < 32) {
#ifdef A2CHAT_HOST
            fprintf(f, "\\u%04x", c);
#else
            {
                static const char hexd[] = "0123456789abcdef";
                fputs("\\u00", f);
                fputc(hexd[c >> 4], f);
                fputc(hexd[c & 15], f);
            }
#endif
        } else {
            fputc(c, f);
        }
    }
}

#ifdef A2CHAT_HOST
unsigned json_escape_len(const char *s, unsigned n)
{
    unsigned i, len = 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i] & 0x7f;
        if (c == 0) {
            continue;
        }
        if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t') {
            len += 2;
        } else if (c < 32) {
            len += 6;
        } else {
            len += 1;
        }
    }
    return len;
}
#endif

#ifdef A2CHAT_HOST
static const char TOOLS_JSON[] =
    "\"tools\":["
    "{\"type\":\"function\",\"function\":{\"name\":\"list_dir\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}}},"
    "{\"type\":\"function\",\"function\":{\"name\":\"read_file\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"},"
    "\"offset\":{\"type\":\"integer\"},"
    "\"length\":{\"type\":\"integer\"}},\"required\":[\"path\"]}}},"
    "{\"type\":\"function\",\"function\":{\"name\":\"write_file\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"},"
    "\"content\":{\"type\":\"string\"},"
    "\"type\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}}},"
    "{\"type\":\"function\",\"function\":{\"name\":\"create_bin\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"},"
    "\"hex\":{\"type\":\"string\"},"
    "\"type\":{\"type\":\"string\"}},\"required\":[\"path\",\"hex\"]}}}]";

void json_write_tools(FILE *f)
{
    fputs("],", f);
    fputs(TOOLS_JSON, f);
}

void json_write_prelude(FILE *f, const char *model)
{
    fputs("{\"model\":\"", f);
            json_escape_fwrite(f, model, (unsigned)strlen(model));
    fputs("\",\"stream\":true,\"think\":false,\"messages\":[", f);
}

void json_write_epilogue(FILE *f)
{
    fputc('}', f);
}
#endif
