#include "a2chat.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static int win_ends(const struct jsonscan *j, const char *suf)
{
    size_t n = strlen(suf);
    if (j->winlen < n) {
        return 0;
    }
    return memcmp(j->win + j->winlen - n, suf, n) == 0;
}

void jsonscan_init(struct jsonscan *j)
{
    memset(j, 0, sizeof(*j));
}

void jsonscan_on_byte(char ch, void *user)
{
    jsonscan_feed((struct jsonscan *)user, ch);
}

static void win_add(struct jsonscan *j, char ch)
{
    if (j->winlen < A2CHAT_WIN) {
        j->win[j->winlen++] = ch;
    } else {
        memmove(j->win, j->win + 1, A2CHAT_WIN - 1);
        j->win[A2CHAT_WIN - 1] = ch;
    }
}

static void emit_content(struct jsonscan *j, char ch)
{
    if (j->on_content) {
        j->on_content(ch, j->user);
    }
}

static void pay_ch(struct jsonscan *j, char ch)
{
    if (j->pay && j->pay_bytes < 65535u) {
        fputc((unsigned char)ch, j->pay);
        j->pay_bytes++;
    }
}

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
    }
    j->numlen = 0;
    j->mode = JS_SEEK;
}

void jsonscan_feed(struct jsonscan *j, char ch)
{
    if (j->mode == JS_NUM_OFFSET || j->mode == JS_NUM_LENGTH || j->mode == JS_NUM_AUX) {
        if (ch >= '0' && ch <= '9' && j->numlen < 7) {
            j->numbuf[j->numlen++] = ch;
            return;
        }
        end_num(j);
        /* fall through to seek with this char */
    }

    if (j->mode == JS_MSG_CONTENT || j->mode == JS_TOOL_NAME ||
        j->mode == JS_ARG_PATH || j->mode == JS_ARG_TYPE ||
        j->mode == JS_ARG_CONTENT || j->mode == JS_ARG_HEX) {
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
                char out = (j->unicode < 128) ? (char)j->unicode : '?';
                if (j->mode == JS_MSG_CONTENT) {
                    emit_content(j, out);
                } else if (j->mode == JS_ARG_CONTENT || j->mode == JS_ARG_HEX) {
                    pay_ch(j, out);
                }
            }
            return;
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
            return;
        }

        if (j->mode == JS_MSG_CONTENT) {
            emit_content(j, ch);
        } else if (j->mode == JS_TOOL_NAME) {
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
        return;
    }

    win_add(j, ch);

    if (win_ends(j, "\"arguments\"")) {
        j->seen_arguments = 1;
    }
    if (win_ends(j, "tool_calls")) {
        j->seen_tool_calls = 1;
    }
    if (win_ends(j, "\"done\":true") || win_ends(j, "\"done\": true")) {
        j->done = 1;
    }
    if (win_ends(j, "\"content\":\"")) {
        j->mode = j->seen_arguments ? JS_ARG_CONTENT : JS_MSG_CONTENT;
        j->escape = 0;
        return;
    }
    if (win_ends(j, "\"name\":\"")) {
        /* llama3.2 often dumps a fake call in message.content; only
         * honor name when this NDJSON object had tool_calls. */
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
}

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
            fprintf(f, "\\u%04x", c);
        } else {
            fputc(c, f);
        }
    }
}

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

#ifndef A2CHAT_HOST
#pragma rodata-name ("LC")
#endif
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
#ifndef A2CHAT_HOST
#pragma rodata-name ("RODATA")
#endif

void json_write_prelude(FILE *f, const char *model)
{
    fputs("{\"model\":\"", f);
            json_escape_fwrite(f, model, (unsigned)strlen(model));
    fputs("\",\"stream\":true,\"think\":false,\"messages\":[", f);
}

void json_write_tools(FILE *f)
{
    /* Close messages[], then sibling "tools":[...] on the root object. */
    fputs("],", f);
    fputs(TOOLS_JSON, f);
}

void json_write_epilogue(FILE *f)
{
    fputc('}', f);
}
