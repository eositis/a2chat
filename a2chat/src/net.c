#include "a2chat.h"
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
    /* Stay in IP65 MACRAW. Do not call w5100_config() — that switches the
     * chip to on-chip TCP and breaks telnet65-style tcp_connect. */
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
    char line[81];

    ui_print(aux_present()
             ? "Stack    IP65 TCP; POST in aux $4000 (32K)"
             : "Stack    IP65 TCP; aux RAM not found");
    ui_nl();
    ui_print("Apple IP ");
    ui_print(dotted_quad(cfg_ip));
    ui_nl();
    ui_print("Gateway  ");
    ui_print(dotted_quad(cfg_gateway));
    ui_nl();
    addr = parse_dotted_quad(g_cfg.host);
    if (!addr) {
        ui_print("HOST is not dotted IPv4: ");
        ui_print(g_cfg.host);
        ui_nl();
        return;
    }
    sprintf(line, "Ollama   %s:%u", g_cfg.host, (unsigned)g_cfg.port);
    ui_print(line);
    ui_nl();
    ui_print("Target   ");
    ui_print(dotted_quad(addr));
    ui_nl();
    if (!g_net_ok) {
        ui_print("Ethernet not up; skip probe");
        ui_nl();
        return;
    }
    rc = http_probe_tags(addr, g_cfg.port);
    ui_redraw_chrome();
    if (rc < 0) {
        ui_print("Probe FAIL: ");
        ui_print(g_http_err[0] ? g_http_err : "unknown");
        ui_nl();
        ui_print("Check OLLAMA_HOST=0.0.0.0:11434 and same LAN/subnet.");
        ui_nl();
        return;
    }
    ui_print("Probe OK: ");
    ui_print(g_http_err);
    ui_nl();
    if (rc == 0) {
        ui_print("Pull the model or fix MODEL= in A2CHAT.CFG");
        ui_nl();
    }
}

void net_shutdown(void)
{
    tcp_close();
    g_net_ok = 0;
}
