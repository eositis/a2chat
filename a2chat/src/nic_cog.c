/*
 * CoGS entry points for the A2CHAT.COG overlay.
 * The card owns DHCP, DNS and TCP. This file is the vector the main
 * program calls; the frame protocol stays in cogs.c.
 */
#pragma optimize      (on)
#pragma static-locals (on)

#include "a2chat.h"
#include "cogs.h"
#include "w5100.h"
#include <ip65.h>
#include <stdio.h>
#include <string.h>

uint16_t timer_jiffy(void);
void timer_init(void);

uint8_t cog_probe(uint8_t slot)
{
    return cogs_probe(slot);
}

int cog_init(uint8_t slot)
{
    uint8_t link = 0;
    uint16_t t0, t1;
    uint32_t ip = 0;
    char msg[40];

    g_net_ok = 0;
    timer_init();
    abort_key = 0x83;
    if (!slot || !cogs_probe(slot)) {
        ui_print("CoGS card not found");
        if (slot) {
            fmt_start(msg, sizeof(msg));
            fmt_s(" in slot ");
            fmt_u(slot);
            fmt_end();
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
        if (cogs_netinfo(&link, &ip) == 0 && link == 2) {
            break;
        }
        if (input_check_for_abort_key() ||
            (uint16_t)(timer_jiffy() - t0) > 20000u) {
            ui_print(link == 1 ? "CoGS: still joining network"
                               : "CoGS: network is down");
            ui_nl();
            return -1;
        }
        t1 = timer_jiffy();
        while ((uint16_t)(timer_jiffy() - t1) < 250) {
        }
    }
    g_net_ok = 1;
    fmt_start(msg, sizeof(msg));
    fmt_s("CoGS IP ");
    fmt_s(dotted_quad(ip));
    fmt_end();
    ui_status(msg);
    return 0;
}

int cog_connect(uint32_t addr, const char *host, uint16_t port)
{
    (void)addr;
    if (!host || !host[0]) {
        return -1;
    }
    if (w5100_connect_name(host, (uint8_t)strlen(host), port)) {
        return 0;
    }
    return -1;
}

int cog_send(const uint8_t *p, uint16_t len)
{
    if (ui_aborted()) {
        w5100_disconnect();
        return -1;
    }
    return cogs_send(p, len);
}

uint8_t cog_recv(uint8_t *dst, uint8_t max)
{
    uint16_t n;

    n = w5100_receive_request();
    if (!n || !max) {
        return 0;
    }
    if (n > max) {
        n = max;
    }
    if (cogs_rx(dst, (uint8_t)n)) {
        return 0; /* card stalled; the socket is closed */
    }
    return (uint8_t)n;
}

uint8_t cog_connected(void)
{
    return (uint8_t)w5100_connected();
}

void cog_disconnect(void)
{
    w5100_disconnect();
}

int cog_localtime(uint32_t *secs)
{
    return cogs_localtime(secs);
}

uint8_t cog_last_error(void)
{
    return cogs_last_error();
}

const char *cog_strerror(uint8_t code)
{
    return cogs_strerror(code);
}

uint8_t cog_should_retry(void)
{
    uint8_t e = cogs_last_error();

    /* DNS failures and refusals will not fix themselves. */
    if (e == 1 || e == 2 || e == 4) {
        return 0;
    }
    return 1;
}

void cog_report(void)
{
    cogs_report();
}

static void cog_put2(char *d, unsigned char v)
{
    unsigned char t = 0;

    while (v >= 10) {
        v = (unsigned char)(v - 10);
        t++;
    }
    d[0] = (char)('0' + t);
    d[1] = (char)('0' + v);
}

/* Card time to HH:MM in dst and the ProDOS date at $BF90. Keeps dst
 * unchanged when the card has no time yet or a socket is open. */
void cog_wall(char *dst)
{
    static const uint8_t mdays[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    uint32_t secs;
    uint16_t days, mins, year;
    uint8_t hour, leap, mon;

    if (!dst || cogs_localtime(&secs)) {
        return;
    }
    secs /= 60u;
    days = (uint16_t)(secs / 1440u);
    mins = (uint16_t)secs - days * 1440u;
    hour = 0;
    while (mins >= 60u) {
        mins = (uint16_t)(mins - 60u);
        hour++;
    }
    cog_put2(dst, hour);
    dst[2] = ':';
    cog_put2(dst + 3, (unsigned char)mins);
    dst[5] = 0;
    year = 4;
    while (days >= 1461u) {
        days = (uint16_t)(days - 1461u);
        year = (uint16_t)(year + 4);
    }
    for (;;) {
        leap = (year & 3) ? 0 : 1;
        if (days < 365u + leap) {
            break;
        }
        days = (uint16_t)(days - (365u + leap));
        year++;
    }
    for (mon = 0; mon < 11; mon++) {
        leap = (uint8_t)(mdays[mon] + (mon == 1 && !(year & 3)));
        if (days < leap) {
            break;
        }
        days = (uint16_t)(days - leap);
    }
    if (year >= 100u) {
        year = (uint16_t)(year - 100u);
    }
    *(volatile uint16_t *)0xBF90 =
        (uint16_t)((year << 9) | ((uint16_t)(mon + 1) << 5) | (days + 1));
    *(volatile unsigned char *)0xBF92 = (unsigned char)mins;
    *(volatile unsigned char *)0xBF93 = hour;
}
