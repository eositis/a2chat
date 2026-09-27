#include "a2chat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ip65.h>
#include <apple2_filetype.h>

static uint16_t ans_len;
static uint8_t ans_n;
static char ans_buf[16];
static void ans_flush(void)
{
    if (!ans_n) {
        return;
    }
    if ((uint16_t)(ans_len + ans_n) < A2CHAT_AUX_POST_MAX) {
        aux_write(ans_len, (const unsigned char *)ans_buf, ans_n);
        ans_len += ans_n;
    }
    ans_n = 0;
}

static void emit_ans(char ch)
{
    ui_print_ch(ch);
    if (ans_n < sizeof(ans_buf)) {
        ans_buf[ans_n++] = ch;
        if (ans_n == sizeof(ans_buf)) {
            ans_flush();
        }
    }
}

static void emit_ans_n(const char *p, unsigned n)
{
    if (!n) {
        return;
    }
    ui_print_n(p, n);
    ans_flush();
    if ((uint16_t)(ans_len + n) < A2CHAT_AUX_POST_MAX) {
        aux_write(ans_len, (const unsigned char *)p, n);
        ans_len += n;
    }
}

static void on_token(char ch, void *user)
{
    (void)user;
    emit_ans(ch);
}

static void on_span(const char *p, unsigned n, void *user)
{
    (void)user;
    emit_ans_n(p, n);
}


static uint8_t bas_lst;
static const char nofile[] = "cannot open ";

/* A2CHAT.DTK runs in the idle RX buffer and writes A2CHAT.LST. */
unsigned char __fastcall__ bas_ovl(const char *path);
/* A2CHAT.TOK tokenizes aux[tok_src, tok_end) into a ProDOS BAS file. */
unsigned char __fastcall__ bas_tok(const char *path);
unsigned char bas_romsave(void);
extern unsigned tok_src;
extern unsigned tok_end;
extern unsigned img_src;
extern unsigned img_end;

/* Append a prompt file at the end of the aux system-prompt buffer. */
static void prompt_add(const char *name, unsigned *plen)
{
    FILE *pf;
    size_t n;

    pf = fopen((char *)name, "rb");
    if (!pf) {
        return;
    }
    if (*plen && *plen < A2CHAT_PROMPT_MAX) {
        g_io80[0] = '\n';
        aux_write((unsigned)(A2CHAT_PROMPT_AUX + *plen),
                  (const unsigned char *)g_io80, 1);
        (*plen)++;
    }
    while (*plen < A2CHAT_PROMPT_MAX &&
           (n = fread(g_io80, 1, sizeof(g_io80), pf)) > 0) {
        if (*plen + (unsigned)n > A2CHAT_PROMPT_MAX) {
            n = A2CHAT_PROMPT_MAX - *plen;
        }
        aux_write((unsigned)(A2CHAT_PROMPT_AUX + *plen),
                  (const unsigned char *)g_io80, (unsigned)n);
        *plen += (unsigned)n;
    }
    fclose(pf);
}

/* Index of a ``` fence at or after `from`, or 0xFFFF. */
static uint16_t find_ticks(uint16_t from)
{
    uint16_t i;
    uint8_t n;

    n = 0;
    for (i = from; i < ans_len; i++) {
        unsigned char b;

        aux_read(i, &b, 1);
        aux_mainbank();
        if ((b & 0x7f) == '`') {
            if (++n == 3) {
                return (uint16_t)(i - 2);
            }
        } else {
            n = 0;
        }
    }
    return 0xffffu;
}

/* Reply text inside the first fence. The opening line (```basic) is skipped. */
static uint16_t fence_body(uint16_t *end)
{
    uint16_t a;
    uint16_t s;
    uint16_t b;
    unsigned char c;

    a = find_ticks(0);
    if (a == 0xffffu) {
        *end = ans_len;
        return 0;
    }
    s = (uint16_t)(a + 3);
    while (s < ans_len) {
        aux_read(s, &c, 1);
        aux_mainbank();
        s++;
        if (c == '\n' || c == '\r') {
            if (s < ans_len) {
                unsigned char nch;
                aux_read(s, &nch, 1);
                aux_mainbank();
                if ((c == '\r' && nch == '\n') || (c == '\n' && nch == '\r')) {
                    s++;
                }
            }
            break;
        }
    }
    b = find_ticks(s);
    *end = (b == 0xffffu) ? ans_len : b;
    return s;
}

/* Opening fence tag is basic, bas, or applesoft. */
static int fence_says_basic(void)
{
    uint16_t a;
    uint16_t i;
    unsigned n;
    char tag[10];
    unsigned char c;

    a = find_ticks(0);
    if (a == 0xffffu) {
        return 0;
    }
    i = (uint16_t)(a + 3);
    n = 0;
    while (i < ans_len) {
        aux_read(i, &c, 1);
        aux_mainbank();
        i++;
        if (c == ' ' || c == '\t') {
            if (n) {
                break;
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            break;
        }
        if (c >= 'a' && c <= 'z') {
            c = (unsigned char)(c - 32);
        }
        if (n + 1 < sizeof(tag)) {
            tag[n++] = (char)c;
        }
    }
    tag[n] = 0;
    if (n == 3 && tag[0] == 'B' && tag[1] == 'A' && tag[2] == 'S') {
        return 1;
    }
    if (n == 5 && tag[0] == 'B' && tag[1] == 'A' && tag[2] == 'S' &&
        tag[3] == 'I' && tag[4] == 'C') {
        return 1;
    }
    return n == 9 && tag[0] == 'A' && tag[1] == 'P' && tag[2] == 'P' &&
           tag[3] == 'L' && tag[4] == 'E' && tag[5] == 'S' && tag[6] == 'O' &&
           tag[7] == 'F' && tag[8] == 'T';
}

static int write_body(const char *user_text, const char *attach_path, uint16_t *jlen)
{
    uint8_t use_lst;
    uint16_t off;
    FILE *af;
    size_t n;
    unsigned plen;

    /* Read prompt files with the file closed before any aux JSON or TCP.
     * FILEIO $BA00 is also eth_outp (NBUFS=1). Do not put a 128-byte
     * buffer on the C stack: ollama_send already holds jsonscan (~130B)
     * and the stack is only $100, ending at FILEIO. */
    use_lst = bas_lst;
    bas_lst = 0;
    plen = 0;
    aux_mainbank();
    prompt_add("A2CHAT.TXT", &plen);
    if (!plen) {
        prompt_add("A2CHAT.TX", &plen);
    }
    prompt_add("A2SOFT.TXT", &plen);
    aux_mainbank();

    off = 0;
    off = aux_add_str(off, "{\"model\":\"");
    off = json_escape_aux(off, g_cfg.model, (unsigned)strlen(g_cfg.model));
    off = aux_add_str(off, "\",\"stream\":true,\"think\":false,\"messages\":[");
    off = aux_add_str(off, "{\"role\":\"system\",\"content\":\"");
    if (plen) {
        unsigned i = 0;
        while (i < plen) {
            unsigned c = (unsigned)sizeof(g_io80);
            if (c > plen - i) {
                c = plen - i;
            }
            aux_read((unsigned)(A2CHAT_PROMPT_AUX + i),
                     (unsigned char *)g_io80, c);
            aux_mainbank();
            off = json_escape_aux(off, g_io80, c);
            i += c;
        }
    } else {
        off = aux_add_str(off, "Follow the user's request.");
    }
    off = aux_add_str(off, "\"}");
    aux_mainbank();
    if (!attach_path || !attach_path[0]) {
        off = hist_emit_json_aux(off);
    }
    aux_mainbank();
    if ((user_text && user_text[0]) || (attach_path && attach_path[0])) {
        off = aux_add_str(off, ",{\"role\":\"user\",\"content\":\"");
        if (user_text && user_text[0]) {
            off = json_escape_aux(off, user_text, (unsigned)strlen(user_text));
        }
        if (attach_path && attach_path[0]) {
            af = fopen(use_lst ? "A2CHAT.LST" : (char *)attach_path, "rb");
            if (!af) {
                return -2;
            }
            off = aux_add_str(off, "\\n\\n--- ");
            off = json_escape_aux(off, attach_path, (unsigned)strlen(attach_path));
            off = aux_add_str(off, " ---\\n");
            {
                unsigned total = 0;
                uint16_t body0 = off;
                while (total < g_cfg.maxread &&
                       (n = fread(g_io80, 1, sizeof(g_io80), af)) > 0) {
                    if (total + (unsigned)n > g_cfg.maxread) {
                        n = g_cfg.maxread - total;
                    }
                    {
                        unsigned i;
                        for (i = 0; i < (unsigned)n; i++) {
                            unsigned char c = (unsigned char)g_io80[i];
                            if (c == '\r') {
                                c = '\n';
                            } else if (c >= 0x80) {
                                c = (unsigned char)(c & 0x7f);
                            }
                            g_io80[i] = (char)c;
                        }
                    }
                    off = json_escape_aux(off, g_io80, (unsigned)n);
                    total += (unsigned)n;
                    if (off >= A2CHAT_AUX_POST_MAX - 8) {
                        break;
                    }
                }
                fclose(af);
                if ((unsigned)(off - body0) < 8u) {
                    return -3;
                }
            }
        }
        off = aux_add_str(off, "\"}");
    }
    off = aux_add_str(off, "]}");
    *jlen = off;
    return off ? 0 : -1;
}

void iobuf_reclaim(void);

int ollama_send(const char *user_text, const char *attach_path)
{
    uint32_t addr;
    struct jsonscan js;
    int rc;
    uint16_t jlen;

    iobuf_reclaim();

#ifndef A2CHAT_HOST
    __asm__("cld");
#endif

    if (!g_net_ok) {
        bas_lst = 0;
        ui_print("Network not ready");
        ui_nl();
        return -1;
    }
    addr = parse_dotted_quad(g_cfg.host);
    if (!addr) {
        bas_lst = 0;
        ui_print("HOST must be dotted IPv4 in v1");
        ui_nl();
        return -1;
    }
    ui_label("AI");

    jsonscan_init(&js);
    js.on_content = on_token;
    js.on_span = on_span;
    rc = write_body(user_text, attach_path, &jlen);
    if (rc == -2) {
        ui_print(nofile);
        ui_print(attach_path);
        ui_nl();
        return -1;
    }
    if (rc == -3) {
        ui_print("file empty or not text");
        ui_nl();
        return -1;
    }
    if (rc < 0) {
        ui_print("POST JSON empty");
        ui_nl();
        return -1;
    }
    ans_len = 0;
    ans_n = 0;
    js.user = 0;
    ui_status("Talking...");
    js.pay = 0;
    aux_mainbank();
    /* Log the prompt before TCP. The POST reuses output_buffer, which is
     * also the 250-character line (the C stack owns $BE00). */
    if (user_text && user_text[0]) {
        hist_append('U', user_text, (uint16_t)strlen(user_text));
    }
    rc = http_post_aux(addr, g_cfg.port, cfg_api_path(&g_cfg, "/api/chat"),
                       jlen, jsonscan_on_bytes, &js);
    ans_flush();
    ui_flush();
    if (rc < 0) {
        ui_print("\n");
        ui_print(g_http_err[0] ? g_http_err : "HTTP error");
        ui_nl();
        return -1;
    }
    ui_nl();
    {
        char line[16];

        sprintf(line, "%u tokens", (unsigned)js.eval_count);
        ui_set_perf(line);
        ui_redraw_chrome();
    }
    if (!js.eval_count && ans_len) {
        ui_print("stream cut off");
        ui_nl();
    }
    hist_append_aux('A', ans_len);
    return 0;
}

static void cfg_edit(void)
{
    char buf[40];
    ui_print("Host [");
    ui_print(g_cfg.host);
    ui_print("]: ");
    ui_prompt(buf, sizeof(buf));
    if (buf[0]) {
        strncpy(g_cfg.host, buf, sizeof(g_cfg.host) - 1);
    }
    ui_print("Port: ");
    ui_prompt(buf, sizeof(buf));
    if (buf[0]) {
        g_cfg.port = (uint16_t)atoi(buf);
    }
    ui_print("Model: ");
    ui_prompt(buf, sizeof(buf));
    if (buf[0]) {
        strncpy(g_cfg.model, buf, sizeof(g_cfg.model) - 1);
    }
    ui_print("Slot 0=auto: ");
    ui_prompt(buf, sizeof(buf));
    if (buf[0]) {
        g_cfg.slot = (uint8_t)atoi(buf);
    }
    cfg_save(&g_cfg, self_path("A2CHAT.CFG"));
    ui_print("Saved config");
    ui_nl();
}

int cmd_handle(char *line)
{
    char *arg = line;
    iobuf_reclaim();
    while (*arg && *arg != ' ') {
        arg++;
    }
    if (*arg) {
        *arg++ = 0;
        while (*arg == ' ') {
            arg++;
        }
    }
    if (!strcmp(line, "/ping") || !strcmp(line, "/p")) {
        net_diag_ollama();
        return 0;
    }
    if (!strcmp(line, "/quit") || !strcmp(line, "/q")) {
        return 1;
    }
    if (!strcmp(line, "/new")) {
        hist_new();
        ui_clear_chat();
        return 0;
    }
    if (!strcmp(line, "/about") || !strcmp(line, "/a")) {
        ui_clear_chat();
        ui_print("A2CHAT version ");
        ui_print(A2CHAT_VERSION);
        ui_nl();
        ui_print("Created by Elmars Ositis");
        ui_nl();
        ui_print(A2CHAT_DATE);
        ui_nl();
        ui_print("http://github.com/eositis/a2chat");
        ui_nl();
        ui_print("Build B");
        ui_print(A2CHAT_BUILD_STR);
        ui_nl();
        return 0;
    }
    if (!strcmp(line, "/config") || !strcmp(line, "/c")) {
        cfg_edit();
        return 0;
    }
    if (!strcmp(line, "/model")) {
        if (arg[0]) {
            strncpy(g_cfg.model, arg, sizeof(g_cfg.model) - 1);
            cfg_save(&g_cfg, self_path("A2CHAT.CFG"));
        }
        ui_print(g_cfg.model);
        ui_nl();
        return 0;
    }
    if (!strcmp(line, "/cat")) {
        char path[A2CHAT_PATH_MAX];
        if (!arg[0]) {
            prodos_list(0, 0, 0);
            return 0;
        }
        path_join_open(path, arg);
        if (!path[0]) {
            ui_print(nofile);
            ui_print(arg);
            ui_nl();
            return 0;
        }
        prodos_list(path, 0, 0);
        return 0;
    }
    if (!strcmp(line, "/load")) {
        char path[A2CHAT_PATH_MAX];
        char *q;

        if (!arg[0]) {
            ui_print("usage: /load NAME [query]");
            ui_nl();
            return 0;
        }
        q = arg;
        while (*q && *q != ' ') {
            q++;
        }
        if (*q) {
            *q++ = 0;
            while (*q == ' ') {
                q++;
            }
        }
        path_join_open(path, arg);
        if (!path[0]) {
            ui_print(nofile);
            ui_print(arg);
            ui_nl();
            return 0;
        }
        bas_lst = bas_ovl(path);
        if (!q[0]) {
            ui_print("Query: ");
            ui_prompt(line, A2CHAT_LINE_MAX);
            q = line;
        }
        ollama_send(q[0] ? q : "Review this file.", path);
        return 0;
    }
    if (!strcmp(line, "/save") || !strcmp(line, "/s")) {
        char path[A2CHAT_PATH_MAX];
        char pick[4];
        uint8_t pt;
        unsigned auxt;
        FILE *f;
        uint16_t off;

        if (!arg[0]) {
            ui_print("usage: /save NAME");
            ui_nl();
            return 0;
        }
        if (!ans_len) {
            ui_print("No reply.");
            ui_nl();
            return 0;
        }
        pt = 0;
        if (fence_says_basic()) {
            ui_print("Save as BASIC? Y/N ");
            ui_prompt(pick, sizeof(pick));
            if (pick[0] == 'Y' || pick[0] == 'y') {
                pt = PRODOS_T_BAS;
                auxt = 0x0801;
            }
        }
        if (!pt) {
            ui_print("1 TXT  2 BAS  3 BIN  4 SYS: ");
            ui_prompt(pick, sizeof(pick));
            switch (pick[0]) {
            case '2':
                pt = PRODOS_T_BAS;
                auxt = 0x0801;
                break;
            case '3':
                pt = PRODOS_T_BIN;
                auxt = 0x2000;
                break;
            case '4':
                pt = PRODOS_T_SYS;
                auxt = 0x2000;
                break;
            default:
                pt = PRODOS_T_TXT;
                auxt = 0;
                break;
            }
        }
        path_join_prefix(path, arg);
        {
            uint16_t end = 0;
            off = fence_body(&end);
            if (off >= end) {
                ui_print("No program between fences");
                ui_nl();
                return 0;
            }
            if (pt == PRODOS_T_BAS) {
                tok_src = off;
                tok_end = end;
                if (!bas_romsave()) {
                    ui_print("Not a BASIC listing");
                    ui_nl();
                    return 0;
                }
                /* Image is the token file Applesoft SAVE would write. */
                off = img_src;
                end = img_end;
                remove(path);
            }
            _filetype = pt;
            _auxtype = (unsigned)auxt;
            f = fopen(path, "wb");
            if (!f) {
                ui_print("Write failed ");
                ui_print(path);
                ui_nl();
                return 0;
            }
            while (off < end) {
                unsigned char buf[16];
                unsigned i;
                uint16_t n = (uint16_t)(end - off);
                if (n > 16) {
                    n = 16;
                }
                aux_read(off, buf, n);
                aux_mainbank();
                if (pt == PRODOS_T_TXT) {
                    for (i = 0; i < n; i++) {
                        if (buf[i] == '\n') {
                            buf[i] = '\r';
                        }
                    }
                }
                fwrite(buf, 1, n, f);
                off = (uint16_t)(off + n);
            }
        }
        fclose(f);
        ui_print("Wrote ");
        ui_print(path);
        ui_nl();
        return 0;
    }
    if (!strcmp(line, "/read") || !strcmp(line, "/r")) {
        char path[A2CHAT_PATH_MAX];
        if (!arg[0]) {
            ui_print("usage: /read PATH");
            ui_nl();
            return 0;
        }
        path_join_open(path, arg);
        bas_lst = bas_ovl(path);
        {
            FILE *tf = fopen(path, "rb");
            if (!tf) {
                ui_print(nofile);
                ui_print(path);
                ui_nl();
                return 0;
            }
            fclose(tf);
        }
        ollama_send("Discuss this file. Do not write a program unless asked.", path);
        return 0;
    }
    ui_print("see the help row");
    ui_nl();
    return 0;
}
