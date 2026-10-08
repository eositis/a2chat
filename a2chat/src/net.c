#include "a2chat.h"
#include "w5100.h"
#include <ip65.h>
#include <string.h>
#include <stdio.h>

uint8_t g_net_ok;
char g_http_err[32];

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

void net_diag_ollama(void)
{
    uint32_t addr;
    int rc;

    addr = parse_dotted_quad(g_cfg.host);
    if (!addr || !g_net_ok) {
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
