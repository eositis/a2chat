#include <string.h>
#include "a2net.h"

void ping_run(void)
{
    uint32_t ip;
    uint16_t count, i, ms;
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

    count = parse_u16(g_share.count);
    if (count == 0)
        count = 4;
    if (count > 20)
        count = 20;

    ui_logf_ip("PING ", ip);
    for (i = 0; i < count; i++) {
        if (input_check_for_abort_key()) {
            ui_log("aborted");
            return;
        }
        ms = icmp_ping(ip);
        if (ms == 0) {
            ui_log("timeout");
        } else {
            p = 0;
            line[p++] = 't';
            line[p++] = '=';
            fmt_u16(nbuf, ms);
            strcpy(line + p, nbuf);
            strcat(line, " ms");
            ui_log(line);
        }
        ip65_process();
    }
    ui_log("done");
}
