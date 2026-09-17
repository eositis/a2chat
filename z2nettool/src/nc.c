#include <string.h>
#include "a2net.h"

static char ncline[80];
static unsigned char nclen;
static unsigned char nc_closed;
static unsigned char udp_got;

static void __fastcall__ nc_tcp_cb(const uint8_t *buf, int16_t len)
{
    int16_t i;
    if (len < 0) {
        if (nclen) {
            ncline[nclen] = 0;
            ui_log(ncline);
            nclen = 0;
        }
        nc_closed = 1;
        return;
    }
    for (i = 0; i < len; i++) {
        char c = (char)buf[i];
        if (c == '\n' || c == '\r') {
            if (nclen) {
                ncline[nclen] = 0;
                ui_log(ncline);
                nclen = 0;
            }
        } else if (nclen < 39 && c >= 0x20 && c < 0x7F) {
            ncline[nclen++] = c;
        } else if (nclen >= 39) {
            ncline[39] = 0;
            ui_log(ncline);
            nclen = 0;
        }
    }
}

static void udp_cb(void)
{
    uint16_t n = udp_recv_len();
    uint16_t i;
    char line[40];
    unsigned char p = 0;
    udp_got = 1;
    ui_logf_ip("UDP from ", udp_recv_src());
    for (i = 0; i < n && p < 39; i++) {
        char c = (char)udp_recv_buf[i];
        if (c >= 0x20 && c < 0x7F)
            line[p++] = c;
        else if (c == '\n' || c == '\r')
            break;
    }
    line[p] = 0;
    if (p)
        ui_log(line);
}

static int is_udp(void)
{
    char a = g_share.proto[0];
    if (a == 'u' || a == 'U')
        return 1;
    return 0;
}

void nc_run(void)
{
    uint32_t ip;
    uint16_t port;
    uint16_t guard;
    uint8_t payload[42];
    uint16_t plen;

    ip = net_resolve(g_share.host);
    if (!ip) {
        ui_log("DNS lookup failed");
        return;
    }
    port = parse_u16(g_share.port);
    if (port == 0) {
        ui_log("need port");
        return;
    }

    plen = 0;
    while (g_share.payload[plen] && plen < 40) {
        payload[plen] = (uint8_t)g_share.payload[plen];
        plen++;
    }

    if (is_udp()) {
        ui_logf_ip("UDP ", ip);
        udp_got = 0;
        if (udp_add_listener(port, udp_cb)) {
            ui_log("udp listen failed");
            return;
        }
        if (plen == 0) {
            payload[0] = 0;
            plen = 1;
        }
        if (udp_send(payload, plen, ip, port, port))
            ui_log("udp send (arp?) retry...");
        ip65_process();
        (void)timer_read();
        udp_send(payload, plen, ip, port, port);
        guard = 0;
        while (!udp_got && guard < 80) {
            if (input_check_for_abort_key())
                break;
            ip65_process();
            guard++;
            (void)timer_read();
        }
        if (!udp_got)
            ui_log("no udp reply");
        udp_remove_listener(port);
        return;
    }

    ui_logf_ip("TCP ", ip);
    nclen = 0;
    nc_closed = 0;
    if (tcp_connect(ip, port, nc_tcp_cb)) {
        ui_log(ip65_strerror(ip65_error));
        return;
    }
    if (plen) {
        payload[plen++] = '\r';
        payload[plen++] = '\n';
        tcp_send(payload, plen);
    }
    guard = 0;
    while (!nc_closed && guard < 200) {
        if (input_check_for_abort_key()) {
            ui_log("aborted");
            break;
        }
        ip65_process();
        guard++;
        (void)timer_read();
    }
    tcp_close();
    ui_log("closed");
}
