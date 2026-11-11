#include "a2chat.h"
#include "nic.h"
#include <ip65.h>
#include <string.h>
#include <stdio.h>

uint8_t g_net_ok;
char g_http_err[32];

void net_diag_ollama(void)
{
    uint32_t addr;
    int rc;

    addr = parse_dotted_quad(g_cfg.host);
    if (!g_net_ok) {
        ui_print("Network not up");
        ui_nl();
        return;
    }
    if (nic_flags() & NIC_F_HOSTNAME) {
        if (!g_cfg.host[0]) {
            ui_print("Set HOST= in A2CHAT.CFG");
            ui_nl();
            return;
        }
    } else if (!addr) {
        ui_print("HOST must be dotted IPv4");
        ui_nl();
        return;
    }
    if (g_cfg.api == A2CHAT_API_AUTO) {
        g_api_lock = 0;
    }
    rc = api_probe(addr, g_cfg.port);
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
        nic_disconnect();
    }
    g_net_ok = 0;
}
