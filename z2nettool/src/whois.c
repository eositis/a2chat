#include <string.h>
#include "a2net.h"

static char linebuf[80];
static unsigned char linelen;
static unsigned char closed_flag;

static void __fastcall__ whois_cb(const uint8_t *buf, int16_t len)
{
    int16_t i;
    if (len < 0) {
        if (linelen) {
            linebuf[linelen] = 0;
            ui_log(linebuf);
            linelen = 0;
        }
        closed_flag = 1;
        return;
    }
    for (i = 0; i < len; i++) {
        char c = (char)buf[i];
        if (c == '\n' || c == '\r') {
            if (linelen) {
                linebuf[linelen] = 0;
                ui_log(linebuf);
                linelen = 0;
            }
        } else if (linelen < 39) {
            if (c >= 0x20 && c < 0x7F)
                linebuf[linelen++] = c;
        } else {
            linebuf[39] = 0;
            ui_log(linebuf);
            linelen = 0;
            if (c >= 0x20 && c < 0x7F)
                linebuf[linelen++] = c;
        }
    }
}

void whois_run(void)
{
    uint32_t ip;
    uint16_t port;
    char q[42];
    unsigned char n;
    uint16_t guard;

    if (!g_share.host[0]) {
        ui_log("empty query");
        return;
    }
    if (!g_share.wserver[0])
        strcpy(g_share.wserver, "whois.iana.org");

    ip = net_resolve(g_share.wserver);
    if (!ip) {
        ui_log("WHOIS server lookup failed");
        return;
    }
    port = parse_u16(g_share.port);
    if (port == 0)
        port = 43;

    ui_logf_ip("WHOIS ", ip);
    linelen = 0;
    closed_flag = 0;
    if (tcp_connect(ip, port, whois_cb)) {
        ui_log(ip65_strerror(ip65_error));
        return;
    }

    n = 0;
    while (g_share.host[n] && n < 40) {
        q[n] = g_share.host[n];
        n++;
    }
    q[n++] = '\r';
    q[n++] = '\n';
    if (tcp_send((uint8_t *)q, n)) {
        ui_log("send failed");
        tcp_close();
        return;
    }

    guard = 0;
    while (!closed_flag && guard < 400) {
        if (input_check_for_abort_key()) {
            ui_log("aborted");
            break;
        }
        ip65_process();
        guard++;
        (void)timer_read();
    }
    tcp_close();
    ui_log("done");
}
