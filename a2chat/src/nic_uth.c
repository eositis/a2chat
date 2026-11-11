/*
 * Uthernet II entry points for the A2CHAT.UTH overlay.
 * Hardware register writes stay in this file so the main program never
 * pokes a slot it has not identified.
 */
#pragma optimize      (on)
#pragma static-locals (on)

#include "a2chat.h"
#include "w5100.h"
#include <ip65.h>
#include <stdio.h>
#include <stdint.h>

uint8_t uth_probe(uint8_t slot)
{
    volatile uint8_t *mode;
    volatile uint8_t *addr_hi;
    volatile uint8_t *addr_lo;
    volatile uint8_t *data;
    uint8_t saved;
    uint8_t a, b;

    if (slot < 1 || slot > 7) {
        return 0;
    }
    mode = (uint8_t *)(slot << 4 | 0xC084);
    addr_hi = mode + 1;
    addr_lo = mode + 2;
    data = mode + 3;
    saved = *mode;
    *mode = 0x03; /* indirect + auto increment */
    *addr_hi = 0x00;
    *addr_lo = 0x17; /* RTR, power-on value 0x07D0 */
    (void)*addr_lo;
    a = *data;
    b = *data;
    *mode = saved;
    return (uint8_t)(a == 0x07 && b == 0xD0);
}

int uth_init(uint8_t slot)
{
    char msg[28];

    g_net_ok = 0;
    ui_status("Initializing Ethernet...");
    if (ip65_init(slot)) {
        ui_print("ip65_init failed: ");
        ui_print(ip65_strerror(ip65_error));
        ui_nl();
        return -1;
    }
    abort_key = 0x83;
    /* DHCP and a static address still use IP65 MACRAW. w5100_config()
     * switches the chip to one TCP socket. Do not call ip65_process
     * or tcp_* after that. */
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
    fmt_start(msg, sizeof(msg));
    fmt_s("IP ");
    fmt_s(dotted_quad(cfg_ip));
    fmt_end();
    ui_status(msg);
    return 0;
}

int uth_connect(uint32_t addr, const char *host, uint16_t port)
{
    (void)host;
    if (w5100_connect_addr(addr, port)) {
        return 0;
    }
    return -1;
}

int uth_send(const uint8_t *p, uint16_t len)
{
    volatile uint8_t *data = w5100_data;

    while (len) {
        uint16_t snd;
        uint16_t i;

        if (ui_aborted()) {
            w5100_disconnect();
            return -1;
        }
        snd = w5100_send_request();
        if (!snd) {
            if (!w5100_connected()) {
                return -1;
            }
            continue;
        }
        if (snd > len) {
            snd = len;
        }
        for (i = 0; i < snd; i++) {
            *data = p[i];
        }
        p += snd;
        len = (uint16_t)(len - snd);
        w5100_send_commit(snd);
    }
    return 0;
}

uint8_t uth_recv(uint8_t *dst, uint8_t max)
{
    volatile uint8_t *data = w5100_data;
    uint16_t n;
    uint8_t i;

    n = w5100_receive_request();
    if (!n || !max) {
        return 0;
    }
    if (n > max) {
        n = max;
    }
    for (i = 0; i < (uint8_t)n; i++) {
        dst[i] = *data;
    }
    w5100_receive_commit(n);
    return (uint8_t)n;
}

uint8_t uth_connected(void)
{
    return (uint8_t)w5100_connected();
}

void uth_disconnect(void)
{
    w5100_disconnect();
}

int uth_localtime(uint32_t *secs)
{
    (void)secs;
    return -1;
}

uint8_t uth_last_error(void)
{
    return 0;
}

const char *uth_strerror(uint8_t code)
{
    (void)code;
    return 0;
}

uint8_t uth_should_retry(void)
{
    return 1;
}

void uth_report(void)
{
}
