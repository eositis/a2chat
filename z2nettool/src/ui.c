#include <string.h>
#include "a2net.h"

#define MACHINE (*(unsigned char *)0xFBB3)

static char outbuf[NTOOLS][OUT_LINES][COLS];
static unsigned char out_len[NTOOLS];
static unsigned char out_head[NTOOLS];
static unsigned char out_view[NTOOLS];

static unsigned char is_iie(void)
{
    return MACHINE == 6;
}

static unsigned char *row_addr(unsigned char row)
{
    return (unsigned char *)(0x0400 + ((row & 7) << 7) + ((row >> 3) * 40));
}

static unsigned char to_screen(unsigned char c, unsigned char inv)
{
    if (c < 0x20 || c > 0x7E)
        c = ' ';
    if (!is_iie() && c >= 0x61 && c <= 0x7A)
        c = (unsigned char)(c - 0x20);
    if (inv)
        return (unsigned char)(c & 0x3F);
    return (unsigned char)(c | 0x80);
}

void ui_cls(void)
{
    unsigned char r;
    for (r = 0; r < ROWS; r++)
        ui_fill_row(r, ' ', 0);
}

void ui_put(unsigned char row, unsigned char col, char c, unsigned char inv)
{
    unsigned char *p;
    if (row >= ROWS || col >= COLS)
        return;
    p = row_addr(row);
    p[col] = to_screen((unsigned char)c, inv);
}

void ui_fill_row(unsigned char row, char c, unsigned char inv)
{
    unsigned char col;
    for (col = 0; col < COLS; col++)
        ui_put(row, col, c, inv);
}

void ui_puts(unsigned char row, unsigned char col, const char *s, unsigned char inv)
{
    while (*s && col < COLS) {
        ui_put(row, col, *s, inv);
        col++;
        s++;
    }
}

void ui_draw_hotkeys(void)
{
    ui_fill_row(0, ' ', 0);
    ui_puts(0, 0, "OA-P/T/N/W/C/M Q=menu X=quit", 0);
}

void ui_draw_sep(void)
{
    ui_fill_row(6, '-', 0);
}

void ui_draw_help(const char *s)
{
    ui_fill_row(23, ' ', 1);
    if (s)
        ui_puts(23, 0, s, 1);
}

static unsigned char tool_index(void)
{
    if (g_tool >= 1 && g_tool <= NTOOLS)
        return (unsigned char)(g_tool - 1);
    return 0;
}

void ui_log(const char *s)
{
    unsigned char t = tool_index();
    unsigned char i;
    char *line;

    line = outbuf[t][out_head[t]];
    for (i = 0; i < COLS; i++)
        line[i] = ' ';
    i = 0;
    while (s && *s && i < COLS) {
        if (*s == '\n' || *s == '\r')
            break;
        line[i++] = *s++;
    }
    out_head[t] = (unsigned char)((out_head[t] + 1) % OUT_LINES);
    if (out_len[t] < OUT_LINES)
        out_len[t]++;
    out_view[t] = 0;
    if (g_tool != TOOL_MENU)
        ui_draw_output();
}

void ui_logf_ip(const char *prefix, uint32_t ip)
{
    char buf[40];
    unsigned char i = 0;
    const char *p = prefix;
    const char *dq;
    while (*p && i < 39)
        buf[i++] = *p++;
    dq = dotted_quad(ip);
    while (*dq && i < 39)
        buf[i++] = *dq++;
    buf[i] = 0;
    ui_log(buf);
}

void ui_draw_output(void)
{
    unsigned char t = tool_index();
    unsigned char vis = OUT_VIS;
    unsigned char start;
    unsigned char i, r, c;
    unsigned char idx;
    unsigned char count = out_len[t];
    unsigned char view = out_view[t];
    char *line;

    if (count <= vis)
        start = 0;
    else {
        if (view > (unsigned char)(count - vis))
            view = (unsigned char)(count - vis);
        start = (unsigned char)(count - vis - view);
    }

    for (i = 0; i < vis; i++) {
        r = (unsigned char)(OUT_ROW0 + i);
        ui_fill_row(r, ' ', 0);
        if (i >= count)
            continue;
        idx = (unsigned char)((out_head[t] + OUT_LINES - count + start + i) % OUT_LINES);
        line = outbuf[t][idx];
        for (c = 0; c < COLS; c++) {
            if (line[c] && line[c] != ' ')
                ui_put(r, c, line[c], 0);
            else if (line[c] == ' ')
                ui_put(r, c, ' ', 0);
        }
    }
}

void ui_scroll(int dir)
{
    unsigned char t = tool_index();
    unsigned char maxv;
    if (out_len[t] <= OUT_VIS)
        return;
    maxv = (unsigned char)(out_len[t] - OUT_VIS);
    if (dir < 0) {
        if (out_view[t] < maxv)
            out_view[t]++;
    } else {
        if (out_view[t])
            out_view[t]--;
    }
    ui_draw_output();
}

void ui_draw_fields(void)
{
    unsigned char i, col, n;
    char *b;
    unsigned char focus_here;
    unsigned char cur;

    for (i = 1; i <= 5; i++)
        ui_fill_row(i, ' ', 0);

    for (i = 0; i < g_nfields; i++) {
        ui_puts(g_fields[i].row, 0, g_fields[i].label, 0);
        col = (unsigned char)strlen(g_fields[i].label);
        if (col < COLS - 1) {
            ui_put(g_fields[i].row, col, ' ', 0);
            col++;
        }
        b = g_fields[i].buf;
        n = 0;
        focus_here = (unsigned char)(!g_in_output && g_focus == i);
        while (b[n] && n < g_fields[i].maxlen && col < COLS) {
            cur = (unsigned char)(focus_here && n == g_cur);
            ui_put(g_fields[i].row, col, b[n], cur);
            n++;
            col++;
        }
        if (focus_here && g_cur >= n && col < COLS)
            ui_put(g_fields[i].row, col, ' ', 1);
    }
}

void ui_draw_menu(void)
{
    unsigned char r;
    char line[40];

    ui_cls();
    ui_draw_hotkeys();
    ui_puts(2, 0, " A2NETTOOL", 0);
    ui_puts(4, 2, "1  PING", 0);
    ui_puts(5, 2, "2  TRACEROUTE", 0);
    ui_puts(6, 2, "3  NSLOOKUP", 0);
    ui_puts(7, 2, "4  WHOIS", 0);
    ui_puts(8, 2, "5  NC", 0);
    ui_puts(9, 2, "6  NMAP", 0);
    ui_puts(10, 2, "7  QUIT", 0);

    ui_fill_row(11, '-', 0);
    ui_puts(12, 0, "A2netTool " A2NET_VERSION, 0);
    ui_puts(13, 0, "TCP/IP stack: IP65", 0);
    ui_puts(14, 0, "64K ProDOS  /  40-col", 0);
    if (g_nic_ok) {
        fmt_nic_line(line);
        ui_puts(15, 0, line, 0);
    } else
        ui_puts(15, 0, "NIC not found (scanned 1-7)", 0);

    if (g_dhcp_ok) {
        fmt_ip_line(line, "IP  ", cfg_ip);
        ui_puts(16, 0, line, 0);
        fmt_ip_line(line, "GW  ", cfg_gateway);
        ui_puts(17, 0, line, 0);
        fmt_ip_line(line, "DNS ", cfg_dns);
        ui_puts(18, 0, line, 0);
    } else {
        ui_puts(16, 0, "DHCP failed", 0);
        if (g_init_err[0])
            ui_puts(17, 0, g_init_err, 0);
    }
    for (r = 19; r < 23; r++)
        ui_fill_row(r, ' ', 0);
    ui_draw_help("1-6 tool   7/X/Q quit   RET run");
}

static const char *field_help(void)
{
    if (g_busy)
        return "ESC abort   waiting...";
    if (g_in_output)
        return "UP/DN scroll output   RET run";
    if (g_focus < g_nfields)
        return g_fields[g_focus].help;
    return "RET run   OA hotkey switches";
}

void ui_draw_tool(void)
{
    ui_cls();
    ui_draw_hotkeys();
    ui_draw_fields();
    ui_draw_sep();
    ui_draw_output();
    ui_draw_help(field_help());
}

void ui_handle_key(unsigned char k)
{
    field_t *f;
    unsigned char len;

    if (g_in_output) {
        if (k == 0x0B)
            ui_scroll(-1);
        else if (k == 0x0A)
            ui_scroll(1);
        else if (k == 0x09 || k == 0x0B) {
            /* tab handled in main */
        }
        return;
    }

    if (g_focus >= g_nfields)
        return;
    f = &g_fields[g_focus];
    len = (unsigned char)strlen(f->buf);
    if (g_cur > len)
        g_cur = len;

    if (k == 0x08) { /* left */
        if (g_cur)
            g_cur--;
        ui_draw_fields();
        return;
    }
    if (k == 0x15) { /* right */
        if (g_cur < len)
            g_cur++;
        ui_draw_fields();
        return;
    }
    if (k == 0x7F || k == 0x08) {
        /* backspace already consumed as left if 0x08; II+ DEL is 0x7F */
    }
    if (k == 0x7F) {
        if (g_cur && len) {
            unsigned char i;
            g_cur--;
            for (i = g_cur; i < len; i++)
                f->buf[i] = f->buf[i + 1];
            ui_draw_fields();
        }
        return;
    }
    if (k >= 0x20 && k < 0x7F) {
        if (len >= f->maxlen)
            return;
        if (g_cur < len) {
            unsigned char i;
            for (i = len; i > g_cur; i--)
                f->buf[i] = f->buf[i - 1];
        }
        f->buf[g_cur] = (char)k;
        f->buf[len + 1] = 0;
        g_cur++;
        ui_draw_fields();
    }
}

static void set_field(unsigned char i, unsigned char id, unsigned char row,
                      unsigned char maxlen, char *buf, const char *label,
                      const char *help)
{
    g_fields[i].id = id;
    g_fields[i].row = row;
    g_fields[i].maxlen = maxlen;
    g_fields[i].buf = buf;
    g_fields[i].label = label;
    g_fields[i].help = help;
}

void ui_setup_tool(unsigned char tool)
{
    g_nfields = 0;
    g_focus = 0;
    g_cur = 0;
    g_in_output = 0;
    g_tool = tool;

    switch (tool) {
    case TOOL_PING:
        set_field(0, FID_HOST, 1, 32, g_share.host, "HOST", "Hostname or dotted IP to ping");
        set_field(1, FID_COUNT, 2, 3, g_share.count, "COUNT", "Number of echo requests");
        set_field(2, FID_TIMEOUT, 3, 5, g_share.timeout, "WAIT", "Timeout hint (ms, IP65 ~2s)");
        g_nfields = 3;
        break;
    case TOOL_TRACE:
        set_field(0, FID_HOST, 1, 32, g_share.host, "HOST", "Hostname or dotted IP to trace");
        set_field(1, FID_MAXHOPS, 2, 2, g_share.max_hops, "HOPS", "Maximum hop count (TTL)");
        set_field(2, FID_TIMEOUT, 3, 5, g_share.timeout, "WAIT", "Per-hop wait in milliseconds");
        g_nfields = 3;
        break;
    case TOOL_NS:
        set_field(0, FID_HOST, 1, 32, g_share.host, "NAME", "DNS name or dotted quad");
        set_field(1, FID_SERVER, 2, 32, g_share.server, "SERVER", "DNS server (blank = DHCP DNS)");
        g_nfields = 2;
        break;
    case TOOL_WHOIS:
        set_field(0, FID_HOST, 1, 32, g_share.host, "QUERY", "Domain or IP for WHOIS");
        set_field(1, FID_SERVER, 2, 32, g_share.wserver, "SERVER", "WHOIS server hostname");
        set_field(2, FID_PORT, 3, 5, g_share.port, "PORT", "TCP port (default 43)");
        g_nfields = 3;
        break;
    case TOOL_NC:
        set_field(0, FID_HOST, 1, 32, g_share.host, "HOST", "Remote host");
        set_field(1, FID_PORT, 2, 5, g_share.port, "PORT", "Remote TCP/UDP port");
        set_field(2, FID_PROTO, 3, 3, g_share.proto, "PROTO", "tcp or udp");
        set_field(3, FID_PAYLOAD, 4, 32, g_share.payload, "DATA", "Payload sent on execute");
        g_nfields = 4;
        break;
    case TOOL_NMAP:
        set_field(0, FID_HOST, 1, 32, g_share.host, "HOST", "Host to probe");
        set_field(1, FID_PORTS, 2, 15, g_share.ports, "PORTS", "Port, list, or range (max 32)");
        g_nfields = 2;
        break;
    default:
        break;
    }

    if (g_nfields && g_fields[0].buf)
        g_cur = (unsigned char)strlen(g_fields[0].buf);
}
