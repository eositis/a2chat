#include "a2chat.h"
#include "w5100.h"
#ifdef A2CHAT_COGS
#include "cogs.h"
#endif
#include <ip65.h>
#include <string.h>
#include <stdio.h>

uint8_t g_net_ok;
char g_http_err[32];

#ifdef A2CHAT_COGS
void timer_init(void);
uint16_t timer_jiffy(void);

/* CoGS: the card owns DHCP / static addressing (set on the card's own
 * config screen), DNS and TCP. IP65 is never initialized here; only its
 * small helpers (dotted_quad, abort key) are linked. */
int net_init(uint8_t slot)
{
    uint8_t link = 0;
    uint16_t t0;
    char msg[40];

    g_net_ok = 0;
    timer_init();
    abort_key = 0x83;
    if (!slot || !cogs_probe(slot)) {
        ui_print("CoGS card not found");
        if (slot) {
            sprintf(msg, " in slot %u", (unsigned)slot);
            ui_print(msg);
        }
        ui_nl();
        return -1;
    }
    ui_status("Initializing CoGS...");
    w5100_init(slot);
    if (g_cfg.ip[0]) {
        ui_print("IP= ignored: set addressing on the CoGS card");
        ui_nl();
    }
    ui_status("Waiting for network...");
    t0 = timer_jiffy();
    for (;;) {
        if (cogs_netinfo(&link, &cfg_ip) == 0 && link == 2) {
            break;
        }
        if (input_check_for_abort_key() ||
            (uint16_t)(timer_jiffy() - t0) > 20000u) {
            ui_print(link == 1 ? "CoGS: still joining network"
                               : "CoGS: network is down");
            ui_nl();
            return -1;
        }
    }
    g_net_ok = 1;
    sprintf(msg, "CoGS IP %s", dotted_quad(cfg_ip));
    ui_status(msg);
    return 0;
}
#else
int net_init(uint8_t slot)
{
    g_net_ok = 0;
    ui_status("Initializing Ethernet...");
    if (ip65_init(slot)) {
        ui_print("ip65_init failed: ");
        ui_print(ip65_strerror(ip65_error));
        ui_nl();
        return -1;
    }
    abort_key = 0x83;
    /* DHCP and static address still use IP65 MACRAW. w5100_config()
     * switches the chip to one 8KB/8KB TCP socket. Do not call
     * ip65_process or tcp_* after that. */
    if (g_cfg.ip[0]) {
        cfg_ip = parse_dotted_quad(g_cfg.ip);
        if (g_cfg.netmask[0]) {
            cfg_netmask = parse_dotted_quad(g_cfg.netmask);
        }
        if (g_cfg.gateway[0]) {
            cfg_gateway = parse_dotted_quad(g_cfg.gateway);
        }
        if (!cfg_ip) {
            ui_print("Bad IP= in config");
            ui_nl();
            return -1;
        }
    } else {
        ui_status("DHCP...");
        if (dhcp_init()) {
            ui_print("dhcp_init failed: ");
            ui_print(ip65_strerror(ip65_error));
            ui_nl();
            return -1;
        }
    }
    w5100_init(slot);
    w5100_config();
    g_net_ok = 1;
    {
        char msg[81];
        sprintf(msg, "IP %s", dotted_quad(cfg_ip));
        ui_status(msg);
    }
    return 0;
}

#endif /* A2CHAT_COGS */

void net_diag_ollama(void)
{
    uint32_t addr;
    int rc;

    addr = parse_dotted_quad(g_cfg.host);
#ifdef A2CHAT_COGS
    /* HOST= may be a hostname; the card resolves it. */
    if (!g_cfg.host[0] || !g_net_ok) {
#else
    if (!addr || !g_net_ok) {
#endif
        ui_print("Ethernet not up");
        ui_nl();
        return;
    }
    rc = http_probe_tags(addr, g_cfg.port);
    ui_redraw_chrome();
    if (rc < 0) {
        ui_print(g_http_err[0] ? g_http_err : "TCP connect failed");
        ui_nl();
        return;
    }
    ui_print("Connection established");
    ui_nl();
}

void net_shutdown(void)
{
    if (g_net_ok) {
        w5100_disconnect();
    }
    g_net_ok = 0;
}
