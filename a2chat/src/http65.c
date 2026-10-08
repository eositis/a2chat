/******************************************************************************
 * HTTP/1.0 over the W5100 TCP socket. IP65 is used only before
 * w5100_config() (DHCP / static address). Do not call ip65_process here.
 ******************************************************************************/

#pragma static-locals (on)
#pragma optimize (on)

#include "a2chat.h"
#include "w5100.h"
#include <ip65.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define BOUNCE 216

static char pkt[216];
#define hdr pkt
#define bod (pkt + 160)
#define bounce ((unsigned char *)pkt)

static uint8_t rx_eof;
static uint8_t rx_have_hdr;
static uint8_t rx_chunked;
static uint8_t rx_chunk_st;
static uint16_t rx_chunk_left;
static uint16_t rx_hlen;
static uint16_t rx_accum_len;
static uint16_t rx_accum_max;
static char rx_hdr[48];
static char rx_hex[8];
static uint8_t rx_hexn;
static void (*rx_on_bytes)(const char *p, unsigned n, void *user);
static void *rx_user;
static char *rx_accum;
static uint8_t rx_hdr_st;

static int http_status_code(const char *hdr)
{
    const char *p = hdr;

    while (*p && *p != ' ') {
        ++p;
    }
    while (*p == ' ') {
        ++p;
    }
    return atoi(p);
}

static void feed_body(char ch)
{
    if (!rx_chunked) {
        if (rx_on_bytes) {
            char b = ch;
            rx_on_bytes(&b, 1, rx_user);
        }
        if (rx_accum && rx_accum_len < rx_accum_max - 1) {
            rx_accum[rx_accum_len++] = ch;
            rx_accum[rx_accum_len] = 0;
        }
        return;
    }
    if (rx_chunk_st == 0) {
        if (ch == '\r') {
            return;
        }
        if (ch == '\n') {
            uint16_t v = 0;
            uint8_t k;

            rx_hex[rx_hexn] = 0;
            for (k = 0; k < rx_hexn; k++) {
                char h = (char)tolower((unsigned char)rx_hex[k]);
                v <<= 4;
                if (h >= '0' && h <= '9') {
                    v |= (uint16_t)(h - '0');
                } else if (h >= 'a' && h <= 'f') {
                    v |= (uint16_t)(h - 'a' + 10);
                }
            }
            rx_hexn = 0;
            if (v == 0) {
                rx_eof = 1;
                return;
            }
            rx_chunk_left = v;
            rx_chunk_st = 1;
            return;
        }
        if (rx_hexn < 7 && isxdigit((unsigned char)ch)) {
            rx_hex[rx_hexn++] = ch;
        }
        return;
    }
    if (rx_chunk_st == 1) {
        if (rx_on_bytes) {
            char b = ch;
            rx_on_bytes(&b, 1, rx_user);
        }
        if (rx_accum && rx_accum_len < rx_accum_max - 1) {
            rx_accum[rx_accum_len++] = ch;
            rx_accum[rx_accum_len] = 0;
        }
        rx_chunk_left--;
        if (rx_chunk_left == 0) {
            rx_chunk_st = 2;
        }
        return;
    }
    if (ch == '\n') {
        rx_chunk_st = 0;
    }
}

static uint16_t rx_last;
static char rxwin[64];

uint16_t timer_jiffy(void);

static int connect_30(uint32_t addr, uint16_t port);

static void feed_byte(char ch)
{
    if (!rx_have_hdr) {
        if (rx_hlen < sizeof(rx_hdr) - 1) {
            rx_hdr[rx_hlen++] = ch;
            rx_hdr[rx_hlen] = 0;
        }
        if (ch == '\r' && (rx_hdr_st == 0 || rx_hdr_st == 2)) {
            rx_hdr_st++;
        } else if (ch == '\n' && (rx_hdr_st == 1 || rx_hdr_st == 3)) {
            rx_hdr_st++;
        } else if (ch == '\r') {
            rx_hdr_st = 1;
        } else {
            rx_hdr_st = 0;
        }
        if (rx_hdr_st == 4) {
            if (strstr(rx_hdr, "chunked") || strstr(rx_hdr, "Chunked")) {
                rx_chunked = 1;
            }
            rx_have_hdr = 1;
        }
        return;
    }
    if (!rx_chunked && rx_on_bytes) {
        rx_on_bytes(&ch, 1, rx_user);
        return;
    }
    feed_body(ch);
}

static uint8_t pump_rx(void)
{
    volatile uint8_t *data = w5100_data;
    uint16_t n;
    uint16_t i;
    uint16_t filled;

    n = w5100_receive_request();
    if (!n) {
        if (!w5100_connected()) {
            rx_eof = 1;
        }
        return 0;
    }
    i = 0;
    while (i < n && !rx_have_hdr) {
        feed_byte((char)*data);
        i++;
    }
    if (rx_chunked || !rx_on_bytes) {
        while (i < n) {
            feed_byte((char)*data);
            i++;
        }
        w5100_receive_commit(n);
        return 1;
    }
    filled = 0;
    while (i < n) {
        if (filled == sizeof(rxwin)) {
            rx_on_bytes(rxwin, filled, rx_user);
            filled = 0;
        }
        rxwin[filled++] = (char)*data;
        i++;
    }
    w5100_receive_commit(n);
    if (filled) {
        rx_on_bytes(rxwin, (unsigned)filled, rx_user);
    }
    return 1;
}

static void rx_reset(void (*on_bytes)(const char *p, unsigned n, void *user),
                     void *user, char *accum, uint16_t accum_max)
{
    rx_eof = 0;
    rx_have_hdr = 0;
    rx_chunked = 0;
    rx_chunk_st = 0;
    rx_chunk_left = 0;
    rx_hlen = 0;
    rx_hdr_st = 0;
    rx_hexn = 0;
    rx_hdr[0] = 0;
    rx_on_bytes = on_bytes;
    rx_user = user;
    rx_accum = accum;
    rx_accum_max = accum_max;
    rx_accum_len = 0;
    if (accum && accum_max) {
        accum[0] = 0;
    }
}

static int send_all(const uint8_t *p, uint16_t len)
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

static int send_aux(uint16_t off, uint16_t total)
{
    while (off < total) {
        uint16_t n = (uint16_t)(total - off);

        if (n > BOUNCE) {
            n = BOUNCE;
        }
        aux_read(off, bounce, n);
        aux_mainbank();
        if (send_all(bounce, n) != 0) {
            return -1;
        }
        off = (uint16_t)(off + n);
    }
    return 0;
}

static int send_str(const char *s)
{
    return send_all((const uint8_t *)s, (uint16_t)strlen(s));
}

static int wait_done(void)
{
    rx_last = timer_jiffy();
    while (!rx_eof) {
        if (ui_aborted()) {
            w5100_disconnect();
            strcpy(g_http_err, "recv aborted");
            return -1;
        }
        if (pump_rx()) {
            rx_last = timer_jiffy();
        } else if ((uint16_t)(timer_jiffy() - rx_last) > 20000u) {
            /* ~20s of vertical blanks with no TCP payload. */
            w5100_disconnect();
            strcpy(g_http_err, "recv timeout");
            return -1;
        }
    }
    w5100_disconnect();
    return 0;
}

static int finish_status(void)
{
    int st;

    if (!rx_have_hdr) {
        strcpy(g_http_err, "TCP up, no HTTP reply");
        return -1;
    }
    st = http_status_code(rx_hdr);
    if (st < 200 || st > 299) {
        const char *p = rx_hdr;
        unsigned i = 0;
        while (*p && *p != '\r' && *p != '\n' && i < sizeof(g_http_err) - 1) {
            g_http_err[i++] = *p++;
        }
        g_http_err[i] = 0;
        return -2;
    }
    return 0;
}

int http_post_aux(uint32_t addr, uint16_t port, const char *url_path,
                  uint16_t json_len,
                  void (*on_bytes)(const char *p, unsigned n, void *user),
                  void *user)
{
    uint16_t hlen;
    int rc;

    if (!json_len) {
        strcpy(g_http_err, "empty POST");
        return -1;
    }

    hlen = (uint16_t)sprintf(hdr,
            "POST %s HTTP/1.0\r\n"
            "Host: %s:%u\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %u\r\n"
            "Connection: close\r\n"
            "\r\n",
            url_path, g_cfg.host, (unsigned)port, (unsigned)json_len);
    if (hlen >= 160) {
        strcpy(g_http_err, "headers too long");
        return -1;
    }

    rx_reset(on_bytes, user, 0, 0);
    aux_mainbank();
    if (connect_30(addr, port)) {
        return -1;
    }
    ui_status(url_path);
    if (send_str(hdr) != 0) {
        w5100_disconnect();
        strcpy(g_http_err, "send hdr");
        return -1;
    }
    if (send_aux(0, json_len) != 0) {
        w5100_disconnect();
        strcpy(g_http_err, "send aux");
        return -1;
    }
    clock_reset_ms();
    if (wait_done() < 0) {
        return -1;
    }
    rc = finish_status();
    if (rc == 0) {
        strcpy(g_http_err, "HTTP 200");
    }
    return rc;
}

/* W5100 RCR makes one connect about 30 seconds. Two retries after that. */
static int connect_30(uint32_t addr, uint16_t port)
{
    uint8_t try;

    for (try = 0; try < 3; try++) {
        if (ui_aborted()) {
            strcpy(g_http_err, "recv aborted");
            return -1;
        }
        ui_status(try ? "Retry connect..." : "Connecting...");
        if (w5100_connect_addr(addr, port)) {
            return 0;
        }
        w5100_disconnect();
    }
    strcpy(g_http_err, "TCP connect failed");
    return -1;
}

int http_probe_tags(uint32_t addr, uint16_t port)
{
    g_http_err[0] = 0;
    rx_reset(0, 0, 0, 0);
    if (connect_30(addr, port)) {
        return -1;
    }
    sprintf(hdr,
            "GET %s HTTP/1.0\r\n"
            "Host: %s:%u\r\n"
            "Connection: close\r\n"
            "\r\n",
            cfg_api_path(&g_cfg, "/api/tags"), g_cfg.host, (unsigned)port);
    ui_status(cfg_api_path(&g_cfg, "/api/tags"));
    if (send_str(hdr) < 0) {
        w5100_disconnect();
        strcpy(g_http_err, "TCP up, send failed");
        return -1;
    }
    if (wait_done() < 0) {
        return -1;
    }
    return finish_status();
}
