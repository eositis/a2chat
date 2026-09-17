#include <string.h>
#include "a2net.h"

static unsigned char closed_now;

static void __fastcall__ nmap_cb(const uint8_t *buf, int16_t len)
{
    (void)buf;
    if (len < 0)
        closed_now = 1;
}

static unsigned char add_port(uint16_t *list, unsigned char n, uint16_t p)
{
    unsigned char i;
    if (p == 0)
        return n;
    for (i = 0; i < n; i++) {
        if (list[i] == p)
            return n;
    }
    if (n >= NMAP_MAX_PORTS)
        return n;
    list[n] = p;
    return (unsigned char)(n + 1);
}

static unsigned char parse_ports(uint16_t *list)
{
    const char *s = g_share.ports;
    unsigned char n = 0;
    uint16_t a, b, p;

    if (!s[0])
        s = "80";

    while (*s && n < NMAP_MAX_PORTS) {
        while (*s == ' ' || *s == ',')
            s++;
        if (!*s)
            break;
        a = 0;
        while (*s >= '0' && *s <= '9') {
            a = (uint16_t)(a * 10 + (*s - '0'));
            s++;
        }
        if (*s == '-') {
            s++;
            b = 0;
            while (*s >= '0' && *s <= '9') {
                b = (uint16_t)(b * 10 + (*s - '0'));
                s++;
            }
            if (b < a) {
                p = a;
                a = b;
                b = p;
            }
            if ((uint16_t)(b - a + 1) > NMAP_MAX_PORTS)
                b = (uint16_t)(a + NMAP_MAX_PORTS - 1);
            for (p = a; p <= b && n < NMAP_MAX_PORTS; p++)
                n = add_port(list, n, p);
        } else {
            n = add_port(list, n, a);
        }
    }
    return n;
}

void nmap_run(void)
{
    uint32_t ip;
    uint16_t ports[NMAP_MAX_PORTS];
    unsigned char n, i;
    char line[40];
    char nbuf[8];

    ip = net_resolve(g_share.host);
    if (!ip) {
        ui_log("DNS lookup failed");
        return;
    }
    n = parse_ports(ports);
    if (n == 0) {
        ui_log("no ports");
        return;
    }

    ui_logf_ip("PROBE ", ip);
    for (i = 0; i < n; i++) {
        if (input_check_for_abort_key()) {
            ui_log("aborted");
            return;
        }
        closed_now = 0;
        fmt_u16(nbuf, ports[i]);
        strcpy(line, nbuf);
        strcat(line, "/tcp ");
        if (tcp_connect(ip, ports[i], nmap_cb)) {
            strcat(line, ip65_error == IP65_ERROR_TIMEOUT_ON_RECEIVE ? "filtered/timeout" : "closed");
            ui_log(line);
        } else {
            strcat(line, "open");
            ui_log(line);
            tcp_close();
        }
        ip65_process();
    }
    ui_log("done");
}
