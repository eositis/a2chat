#include "a2chat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <ip65.h>
#include <apple2_filetype.h>

#define MAX_HOPS 8

static const char SYS_PROMPT[] =
    "Apple IIe ProDOS. No C: paths. "
    "ProDOS names: 15 chars, start with a letter, A-Z 0-9 and one period, no spaces. "
    "Save as <<A2W NAME TYP>>file<<A2E>> TYP TXT, BAS, or BIN.";

static const char WPRE[] = "<<A2W ";
static const char WEND[] = "<<A2E>>";

static uint16_t ans_len;
static uint8_t ans_n;
static char ans_buf[32];
static uint8_t wm;
static uint8_t wmode;
static uint8_t wendm;
static uint8_t wgot;
static uint8_t wpn;
static uint8_t wtn;
static FILE *wpay;
static char scratch[96];

static void wreset(void)
{
    wm = 0;
    wmode = 0;
    wendm = 0;
    wgot = 0;
    wpn = 0;
    wtn = 0;
    scratch[0] = 0;
    scratch[80] = 0;
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
    ui_print_ch(ch);
    if (ans_n < sizeof(ans_buf)) {
        ans_buf[ans_n++] = ch;
        if (ans_n == sizeof(ans_buf)) {
            ans_flush();
        }
    }
}

static void on_token(char ch, void *user)
{
    (void)user;
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
        return;
    }
    if (wmode == 1) {
        if (ch == ' ') {
            wmode = 2;
            wtn = 0;
            scratch[80] = 0;
            return;
        }
        if (wpn + 1 < 80) {
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
            scratch[80 + wtn] = ch;
            wtn++;
            scratch[80 + wtn] = 0;
        }
        return;
    }
    if (wmode == 5) {
        wmode = 3;
        wendm = 0;
        wpay = fopen(self_path("A2CHAT.WR"), "wb");
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
    pt = scratch[80] ? type_from_arg(scratch + 80) : type_from_path(path);
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
    char src[A2CHAT_PATH_MAX];
    FILE *f;
    uint16_t off;
    int wr;

    if (!user_text || !ans_len) {
        return;
    }
    if (!has_word(user_text, ".md") && !has_word(user_text, "save") &&
        !has_word(user_text, "write") && !has_word(user_text, ".bas") &&
        !has_word(user_text, "disk")) {
        return;
    }
    if (has_word(user_text, ".bas")) {
        memcpy(scratch, "PROG.BAS", 9);
    } else {
        memcpy(scratch, "NOTE.MD", 8);
    }
    strncpy(src, self_path("A2CHAT.WR"), sizeof(src) - 1);
    src[sizeof(src) - 1] = 0;
    f = fopen(src, "wb");
    if (!f) {
        ui_print("Write failed");
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
    path_join_prefix(path, scratch);
    wr = has_word(user_text, ".bas")
             ? prodos_write_bas(path, src, 0)
             : prodos_write_file(path, src, PRODOS_T_TXT, 0, 0);
    if (wr < 0) {
        ui_print("Write failed ");
        ui_print(path);
        ui_nl();
        return;
    }
    ui_print("Wrote ");
    ui_print(path);
    ui_nl();
}

static int write_body(const char *attach_path)
{
    FILE *f;
    FILE *af;
    char abuf[128];
    size_t n;

    f = fopen(self_path("A2CHAT.BOD"), "wb");
    if (!f) {
        return -1;
    }
    json_write_prelude(f, g_cfg.model);
    fputs("{\"role\":\"system\",\"content\":\"", f);
    json_escape_fwrite(f, SYS_PROMPT, (unsigned)strlen(SYS_PROMPT));
    if (g_cfg.prefix[0]) {
        static const char pfx[] =
            " Data directory PREFIX=";
        static const char tail[] =
            ". Root. <<A2W NAME TYP>>file<<A2E>>. No /VOLUME.";
        json_escape_fwrite(f, pfx, (unsigned)(sizeof(pfx) - 1));
        json_escape_fwrite(f, g_cfg.prefix, (unsigned)strlen(g_cfg.prefix));
        json_escape_fwrite(f, tail, (unsigned)(sizeof(tail) - 1));
    } else {
        static const char nopfx[] =
            " Workspace is the launch folder. Do not invent /VOLUME as the root.";
        json_escape_fwrite(f, nopfx, (unsigned)(sizeof(nopfx) - 1));
    }
    fputs("\"}", f);
    hist_write_messages(f);
    if (attach_path && attach_path[0]) {
        fputs(",{\"role\":\"user\",\"content\":\"FILE ", f);
        json_escape_fwrite(f, attach_path, (unsigned)strlen(attach_path));
        fputs(":\\n", f);
        af = fopen(attach_path, "rb");
        if (af) {
            unsigned total = 0;
            while (total < g_cfg.maxread &&
                   (n = fread(abuf, 1, sizeof(abuf), af)) > 0) {
                if (total + n > g_cfg.maxread) {
                    n = g_cfg.maxread - total;
                }
                json_escape_fwrite(f, abuf, (unsigned)n);
                total += (unsigned)n;
            }
            fclose(af);
        }
        fputs("\"}", f);
    }
    fputs("]}", f);
    fclose(f);
    return 0;
}

int ollama_send(const char *user_text, const char *attach_path)
{
    uint32_t addr;
    int hop;
    const char *att = attach_path;

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
        hist_append('U', user_text, (uint16_t)strlen(user_text));
        ui_label("You");
        ui_print(user_text);
        ui_nl();
    }
    ui_label("AI");

    for (hop = 0; hop < MAX_HOPS; hop++) {
        struct jsonscan js;
        FILE *pay;
        int rc;

        jsonscan_init(&js);
        js.on_content = on_token;
        if (write_body(att) < 0) {
            ui_print("body write failed: ");
            ui_print(self_path("A2CHAT.BOD"));
            ui_nl();
            return -1;
        }
        att = 0;
        ans_len = 0;
        ans_n = 0;
        wreset();
        js.user = 0;
        ui_status("Talking to Ollama...");
        js.pay = 0;
        rc = http_post_file(addr, g_cfg.port, "/api/chat",
                            self_path("A2CHAT.BOD"), jsonscan_on_byte, &js);
        pay = js.pay;
        if (pay) {
            fclose(pay);
            js.pay = 0;
        }
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
        save_marked_file();
        if (!wgot) {
            save_reply_file(user_text);
        }
        hist_append_aux('A', ans_len);
        ui_redraw_chrome();
        return 0;
    }
    ui_redraw_chrome();
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
        path_join_prefix(path, arg[0] ? arg : ".");
        ui_print(path);
        ui_nl();
        if (prodos_list(path, scratch, sizeof(scratch)) < 0) {
            ui_print(scratch);
            ui_nl();
            return 0;
        }
        ui_print(scratch[0] ? scratch : "(empty)");
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
        path_join_prefix(path, arg);
        ollama_send("Read this file and use it as context.", path);
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
    ui_print("commands: /config /read /cat /model /ping /new /save /quit");
    ui_nl();
    return 0;
}
