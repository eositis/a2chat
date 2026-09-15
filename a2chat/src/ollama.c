#include "a2chat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <ip65.h>
#include <apple2_filetype.h>
static const char WPRE[] = "<<A2W ";
static const char WEND[] = "<<A2E>>";

static uint16_t ans_len;
static uint8_t ans_n;
static char ans_buf[16];
static uint8_t wm;
static uint8_t wmode;
static uint8_t wendm;
static uint8_t wgot;
static uint8_t wpn;
static uint8_t wtn;
static FILE *wpay;
#define scratch g_io80
static uint8_t saw_first;
static uint32_t t_first;

static void wreset(void)
{
    wm = 0;
    wmode = 0;
    wendm = 0;
    wgot = 0;
    wpn = 0;
    wtn = 0;
    scratch[0] = 0;
    scratch[64] = 0;
    if (wpay) {
        fclose(wpay);
        wpay = 0;
    }
}

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
    if (!saw_first) {
        saw_first = 1;
        t_first = clock_elapsed_ms();
    }
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
    if (!saw_first) {
        saw_first = 1;
        t_first = clock_elapsed_ms();
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
    if (wmode == 0 && wm == 0 && ch != '<') {
        emit_ans(ch);
        return;
    }
    if (wmode == 3) {
        if (ch == WEND[wendm]) {
            wendm++;
            if (WEND[wendm] == 0) {
                wgot = 1;
                wmode = 0;
                wendm = 0;
                wm = 0;
                if (wpay) {
                    fclose(wpay);
                    wpay = 0;
                }
            }
            return;
        }
        if (wendm && wpay) {
            fwrite(WEND, 1, wendm, wpay);
            wendm = 0;
            if (ch == WEND[0]) {
                wendm = 1;
                return;
            }
        }
        if (wpay) {
            fputc((unsigned char)ch, wpay);
        }
        emit_ans(ch);
        return;
    }
    if (wmode == 1) {
        if (ch == ' ') {
            wmode = 2;
            wtn = 0;
            scratch[64] = 0;
            return;
        }
        if (wpn + 1 < 64) {
            scratch[wpn++] = ch;
            scratch[wpn] = 0;
        }
        return;
    }
    if (wmode == 2) {
        if (ch == '>') {
            wmode = 5;
            return;
        }
        if (wtn + 1 < 6) {
            scratch[64 + wtn] = ch;
            wtn++;
            scratch[64 + wtn] = 0;
        }
        return;
    }
    if (wmode == 5) {
        wmode = 3;
        wendm = 0;
        /* Do not fopen during TCP: ProDOS on an 8MB volume stalls
         * Virtual ][ and drops the HTTP stream. */
        return;
    }
    if (ch == WPRE[wm]) {
        wm++;
        if (WPRE[wm] == 0) {
            wmode = 1;
            wpn = 0;
            scratch[0] = 0;
            wm = 0;
        }
        return;
    }
    if (wm) {
        uint8_t k;
        for (k = 0; k < wm; k++) {
            emit_ans(WPRE[k]);
        }
        wm = 0;
        if (ch == WPRE[0]) {
            wm = 1;
            return;
        }
    }
    emit_ans(ch);
}

static void on_span(const char *p, unsigned n, void *user)
{
    unsigned i;

    (void)user;
    if (!n) {
        return;
    }
    if (wmode || wm) {
        for (i = 0; i < n; i++) {
            on_token(p[i], 0);
        }
        return;
    }
    for (i = 0; i < n; i++) {
        if (p[i] == '<') {
            for (i = 0; i < n; i++) {
                on_token(p[i], 0);
            }
            return;
        }
    }
    emit_ans_n(p, n);
}

static uint8_t type_from_arg(const char *t)
{
    if (!t || !t[0]) {
        return PRODOS_T_TXT;
    }
    if (!strcmp(t, "SYS") || !strcmp(t, "sys") || !strcmp(t, "FF")) {
        return PRODOS_T_SYS;
    }
    if (!strcmp(t, "BIN") || !strcmp(t, "bin") || !strcmp(t, "06")) {
        return PRODOS_T_BIN;
    }
    if (!strcmp(t, "BAS") || !strcmp(t, "bas") || !strcmp(t, "FC") ||
        !strcmp(t, "fc")) {
        return PRODOS_T_BAS;
    }
    if (!strcmp(t, "MD") || !strcmp(t, "md")) {
        return PRODOS_T_TXT;
    }
    return PRODOS_T_TXT;
}

static uint8_t type_from_path(const char *path)
{
    const char *d;
    char ext[5];
    unsigned i;

    d = strrchr(path, '.');
    if (!d || !d[1]) {
        return 0;
    }
    for (i = 0; i < 4 && d[i + 1]; i++) {
        char c = d[i + 1];
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 32);
        }
        ext[i] = c;
    }
    ext[i] = 0;
    if (!strcmp(ext, "BAS")) {
        return PRODOS_T_BAS;
    }
    if (!strcmp(ext, "BIN") || !strcmp(ext, "SYS")) {
        return PRODOS_T_BIN;
    }
    return PRODOS_T_TXT;
}

static int confirm_write(char *path, const char *kind, unsigned bytes, uint8_t ptype)
{
    int r;
    char edit[A2CHAT_PATH_MAX];

    /* PREFIX (or cwd if PREFIX is empty) is the session workspace: text
     * work files there do not need a prompt. Confirm generated BIN/SYS
     * and anything outside the workspace. */
    if (ptype != PRODOS_T_BIN && ptype != PRODOS_T_SYS &&
        path_in_workspace(path)) {
        return 1;
    }
    r = ui_confirm(path, kind, bytes);
    if (r == 1) {
        return 1;
    }
    if (r == 2) {
        ui_print("\nNew path: ");
        ui_prompt(edit, A2CHAT_PATH_MAX);
        if (edit[0]) {
            path_join_prefix(path, edit);
            return 1;
        }
    }
    return 0;
}

#if 0
static int run_tool(struct jsonscan *js, char *result, unsigned rsz)
{
    char path[A2CHAT_PATH_MAX];
    unsigned got = 0;
    const char *pay = self_path("A2CHAT.PAY");
    unsigned i;

    result[0] = 0;
    {
        unsigned n;
        for (n = 0; js->tool_name[n]; n++) {
            char c = js->tool_name[n];
            if (c >= 'A' && c <= 'Z') {
                js->tool_name[n] = (char)(c + 32);
            }
        }
    }
    path_join_prefix(path, js->arg_path[0] ? js->arg_path : ".");
    for (i = 0; path[i]; i++) {
        if (path[i] == ' ' || path[i] == '\\' || path[i] == ':') {
            strncpy(result, "ProDOS path required, not C: or DOS", rsz - 1);
            return 0;
        }
    }

    if (!strcmp(js->tool_name, "list_dir")) {
        if (prodos_list(path, result, rsz) < 0) {
            return -1;
        }
        return 0;
    }
    if (!strcmp(js->tool_name, "read_file")) {
        unsigned len = js->arg_length ? js->arg_length : g_cfg.maxread;
        int hex = 0;
        uint8_t pt;

        pt = js->arg_type[0] ? type_from_arg(js->arg_type) : type_from_path(path);
        if (pt == PRODOS_T_BIN || pt == PRODOS_T_SYS) {
            hex = 1;
        }
        if (prodos_read_to_aux(path, js->arg_offset, len, &got, hex) < 0) {
            strncpy(result, "cannot open", rsz - 1);
            return -1;
        }
        hist_append_aux('T', (uint16_t)got);
        result[0] = 0;
        ui_print("Read ");
        ui_print(path);
        ui_nl();
        return 0;
    }
    if (!strcmp(js->tool_name, "write_file")) {
        unsigned bytes = js->pay_bytes;
        uint8_t pt = type_from_arg(js->arg_type);
        int wr;

        if (!js->arg_type[0]) {
            uint8_t fromp = type_from_path(path);
            if (fromp) {
                pt = fromp;
            }
        }
        if (!confirm_write(path, "write_file", bytes, pt)) {
            strncpy(result, "user declined write", rsz - 1);
            return 0;
        }
        if (pt == PRODOS_T_BAS) {
            wr = prodos_write_bas(path, pay, 0);
        } else if (pt == PRODOS_T_BIN || pt == PRODOS_T_SYS) {
            unsigned aux = js->arg_auxtype ? js->arg_auxtype : 0x2000;
            wr = prodos_write_hex_file(path, pay, pt, aux, 0);
        } else {
            wr = prodos_write_file(path, pay, pt, 0, 0);
        }
        if (wr < 0) {
            strncpy(result, "write failed", rsz - 1);
            return -1;
        }
        ui_print("Wrote ");
        ui_print(path);
        ui_nl();
        strncpy(result, "wrote ok ", rsz - 1);
        strncat(result, path, rsz - strlen(result) - 1);
        return 0;
    }
    if (!strcmp(js->tool_name, "create_bin")) {
        unsigned aux = js->arg_auxtype ? js->arg_auxtype : 0x2000;
        uint8_t pt = type_from_arg(js->arg_type);
        if (pt == PRODOS_T_TXT) {
            pt = PRODOS_T_BIN;
        }
        if (!confirm_write(path, "create_bin", js->pay_bytes / 2, pt)) {
            strncpy(result, "user declined write", rsz - 1);
            return 0;
        }
        if (prodos_write_hex_file(path, pay, pt, aux, 0) < 0) {
            strncpy(result, "create_bin failed", rsz - 1);
            return -1;
        }
        strncpy(result, "bin ok", rsz - 1);
        return 0;
    }
    strncpy(result, "unknown tool", rsz - 1);
    return -1;
}
#endif

static void save_marked_file(void)
{
    char path[A2CHAT_PATH_MAX];
    char src[A2CHAT_PATH_MAX];
    uint8_t pt;
    int wr;

    if (!wgot || !scratch[0]) {
        return;
    }
    strncpy(src, self_path("A2CHAT.WR"), sizeof(src) - 1);
    src[sizeof(src) - 1] = 0;
    path_join_prefix(path, scratch);
    pt = scratch[64] ? type_from_arg(scratch + 64) : type_from_path(path);
    if (!pt) {
        pt = PRODOS_T_TXT;
    }
    if (!confirm_write(path, "write", 1, pt)) {
        ui_print("Write skipped");
        ui_nl();
        return;
    }
    if (pt == PRODOS_T_BAS) {
        wr = prodos_write_bas(path, src, 0);
    } else if (pt == PRODOS_T_BIN || pt == PRODOS_T_SYS) {
        wr = prodos_write_hex_file(path, src, pt, 0x2000, 0);
    } else {
        wr = prodos_write_file(path, src, pt, 0, 0);
    }
    if (wr < 0) {
        ui_print("Write failed");
        ui_nl();
        return;
    }
    ui_print("Wrote ");
    ui_print(path);
    ui_nl();
}

static int has_word(const char *t, const char *w)
{
    unsigned i, j;

    if (!t || !w) {
        return 0;
    }
    for (i = 0; t[i]; i++) {
        for (j = 0; w[j]; j++) {
            char a = t[i + j];
            if (a >= 'A' && a <= 'Z') {
                a = (char)(a + 32);
            }
            if (a != w[j]) {
                break;
            }
        }
        if (!w[j]) {
            return 1;
        }
    }
    return 0;
}

static void save_reply_file(const char *user_text)
{
    char path[A2CHAT_PATH_MAX];
    FILE *f;
    uint16_t off;

    if (!user_text || !ans_len) {
        return;
    }
    if (!has_word(user_text, ".md") && !has_word(user_text, "save") &&
        !has_word(user_text, ".bas") && !has_word(user_text, "disk")) {
        return;
    }
    if (has_word(user_text, ".bas")) {
        memcpy(scratch, "PROG.BAS", 9);
    } else {
        memcpy(scratch, "NOTE.MD", 8);
    }
    path_join_prefix(path, scratch);
    _filetype = has_word(user_text, ".bas") ? PRODOS_T_BAS : PRODOS_T_TXT;
    _auxtype = has_word(user_text, ".bas") ? 0x0801 : 0;
    f = fopen(path, "wb");
    if (!f && g_cfg.prefix[0]) {
        strncpy(path, scratch, sizeof(path) - 1);
        path[sizeof(path) - 1] = 0;
        f = fopen(path, "wb");
    }
    if (!f) {
        ui_print("Write failed ");
        ui_print(path);
        ui_nl();
        return;
    }
    off = 0;
    while (off < ans_len) {
        unsigned char buf[16];
        uint16_t n = (uint16_t)(ans_len - off);
        if (n > 16) {
            n = 16;
        }
        aux_read(off, buf, n);
        fwrite(buf, 1, n, f);
        off += n;
    }
    fclose(f);
    ui_print("Wrote ");
    ui_print(path);
    ui_nl();
}

static int write_body(const char *user_text, const char *attach_path, uint16_t *jlen)
{
    uint16_t off;
    FILE *af;
    FILE *pf;
    size_t n;
    unsigned plen;

    /* Read A2CHAT.TXT with the file closed before any aux JSON or TCP.
     * FILEIO $BA00 is also eth_outp (NBUFS=1). Do not put a 128-byte
     * buffer on the C stack: ollama_send already holds jsonscan (~130B)
     * and the stack is only $100, ending at FILEIO. */
    plen = 0;
    aux_mainbank();
    pf = fopen("A2CHAT.TXT", "rb");
    if (!pf) {
        pf = fopen("A2CHAT.TX", "rb");
    }
    if (pf) {
        while (plen < A2CHAT_PROMPT_MAX &&
               (n = fread(g_io80, 1, sizeof(g_io80), pf)) > 0) {
            if (plen + (unsigned)n > A2CHAT_PROMPT_MAX) {
                n = A2CHAT_PROMPT_MAX - plen;
            }
            aux_write((unsigned)(A2CHAT_PROMPT_AUX + plen),
                      (const unsigned char *)g_io80, (unsigned)n);
            plen += (unsigned)n;
        }
        fclose(pf);
    }
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
            af = fopen((char *)attach_path, "rb");
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

int ollama_send(const char *user_text, const char *attach_path)
{
    uint32_t addr;
    struct jsonscan js;
    int rc;
    uint16_t jlen;

#ifndef A2CHAT_HOST
    __asm__("cld");
#endif

    if (!g_net_ok) {
        ui_print("Network not ready");
        ui_nl();
        return -1;
    }
    addr = parse_dotted_quad(g_cfg.host);
    if (!addr) {
        ui_print("HOST must be dotted IPv4 in v1");
        ui_nl();
        return -1;
    }
    if (user_text && user_text[0]) {
        ui_label("You");
        ui_print(user_text);
        ui_nl();
    }
    ui_label("AI");

    jsonscan_init(&js);
    js.on_content = on_token;
    js.on_span = on_span;
    rc = write_body(user_text, attach_path, &jlen);
    if (rc == -2) {
        ui_print("cannot open ");
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
    saw_first = 0;
    t_first = 0;
    wreset();
    js.user = 0;
    ui_status("Talking to Ollama...");
    js.pay = 0;
    aux_mainbank();
    rc = http_post_aux(addr, g_cfg.port, "/api/chat",
                       jlen, jsonscan_on_bytes, &js);
    if (wpay) {
        fclose(wpay);
        wpay = 0;
    }
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
        uint32_t dt;
        unsigned sec;
        unsigned tok;
        unsigned tps;
        char line[40];

        dt = clock_post_ms();
        sec = (unsigned)(dt / 1000ul);
        if (sec == 0) {
            sec = 1;
        }
        tok = (unsigned)js.eval_count;
        tps = tok / sec;
        sprintf(line, "%us %ut %u/s", sec, tok, tps);
        ui_set_perf(line);
        ui_redraw_chrome();
    }
    if (!js.eval_count && ans_len) {
        ui_print("stream cut off");
        ui_nl();
    }
    save_marked_file();
    if (!wgot) {
        save_reply_file(user_text);
    }
    if (user_text && user_text[0]) {
        hist_append('U', user_text, (uint16_t)strlen(user_text));
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
    ui_print("Saved config. Restart to re-init net if slot/IP changed.");
    ui_nl();
}

int cmd_handle(char *line)
{
    char *arg = line;
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
        ui_print("14 September 2026");
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
        path_join_open(path, arg[0] ? arg : ".");
        prodos_list(path, 0, 0);
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
        {
            FILE *tf = fopen(path, "rb");
            if (!tf) {
                ui_print("cannot open ");
                ui_print(path);
                ui_nl();
                return 0;
            }
            fclose(tf);
        }
        ollama_send("Discuss this file. Do not write a program unless asked.", path);
        return 0;
    }
    if (!strcmp(line, "/save") || !strcmp(line, "/s")) {
        char path[A2CHAT_PATH_MAX];
        if (!arg[0]) {
            ui_print("usage: /save PATH");
            ui_nl();
            return 0;
        }
        path_join_prefix(path, arg);
        prodos_write_file(path, self_path("A2CHAT.LOG"), PRODOS_T_TXT, 0, 1);
        ui_print("Saved log");
        ui_nl();
        return 0;
    }
    ui_print("commands: /config /cat /model /ping /new /quit /about");
    ui_nl();
    return 0;
}
