#include <string.h>
#include "a2net.h"

void traceroute_run(void)
{
    uint32_t ip;
    uint16_t hops, wait, h;
    uint8_t rc;
    char nbuf[8];
    char line[40];
    unsigned char p;

    ip = net_resolve(g_share.host);
    if (!ip) {
        ui_log("DNS lookup failed");
        if (ip65_error)
            ui_log(ip65_strerror(ip65_error));
        return;
    }

    hops = parse_u16(g_share.max_hops);
    if (hops == 0)
        hops = 16;
    if (hops > 30)
        hops = 30;
    wait = parse_u16(g_share.timeout);
    if (wait < 200)
        wait = 2000;

    ui_logf_ip("TRACE ", ip);
    icmp_trace_dest = ip;

    for (h = 1; h <= hops; h++) {
        if (input_check_for_abort_key()) {
            ui_log("aborted");
            return;
        }
        icmp_trace_ttl = (uint8_t)h;
        rc = icmp_trace_hop(wait);
        p = 0;
        fmt_u16(nbuf, h);
        line[p++] = nbuf[0];
        if (nbuf[1])
            line[p++] = nbuf[1];
        line[p++] = ' ';
        line[p] = 0;
        if (rc == 0) {
            strcat(line, "*");
            ui_log(line);
        } else {
            strcat(line, dotted_quad(icmp_trace_from));
            ui_log(line);
            if (rc == 2) {
                ui_log("reached");
                return;
            }
        }
    }
    ui_log("max hops");
}
