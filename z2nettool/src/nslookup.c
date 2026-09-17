#include "a2net.h"

void nslookup_run(void)
{
    uint32_t saved, ip, srv;

    saved = cfg_dns;
    if (g_share.server[0]) {
        srv = net_resolve(g_share.server);
        if (!srv) {
            ui_log("bad DNS server");
            return;
        }
        cfg_dns = srv;
    }

    ui_logf_ip("SERVER ", cfg_dns);
    ip = net_resolve(g_share.host);
    cfg_dns = saved;

    if (!ip) {
        ui_log("NXDOMAIN / lookup failed");
        if (ip65_error)
            ui_log(ip65_strerror(ip65_error));
        return;
    }
    ui_logf_ip("A     ", ip);
}
