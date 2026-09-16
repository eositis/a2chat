/******************************************************************************
 * HTTP/1.0 over IP65 software TCP (same path as telnet65 / test/tcp.c).
 * wget65's W5100 on-chip TCP is not used: w5100_config() leaves MACRAW,
 * which is what IP65 needs for tcp_connect.
 ******************************************************************************/

#pragma static-locals (on)

#include "a2chat.h"
#include <ip65.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define TCP_MAX 900
#define BOUNCE 900

static char pkt[900];
#define hdr pkt
#define bod (pkt + 200)
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

static void __fastcall__ on_tcp(const uint8_t *buf, int16_t len)
{
    uint16_t i;

    rx_last = timer_read();
    if (len < 0) {
        rx_eof = 1;
        return;
    }
    i = 0;
    while (i < (uint16_t)len && !rx_have_hdr) {
        char ch = (char)buf[i++];

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
    }
    if (rx_have_hdr && i < (uint16_t)len) {
        if (!rx_chunked && rx_on_bytes) {
            rx_on_bytes((const char *)buf + i, (unsigned)((uint16_t)len - i),
                        rx_user);
        } else {
            while (i < (uint16_t)len) {
                feed_body((char)buf[i++]);
            }
        }
    }
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

static void send_fail(const char *what)
{
    strncpy(g_http_err, what, sizeof(g_http_err) - 1);
    g_http_err[sizeof(g_http_err) - 1] = 0;
    if (strlen(g_http_err) + 2 < sizeof(g_http_err)) {
        strncat(g_http_err, " ", sizeof(g_http_err) - strlen(g_http_err) - 1);
        strncat(g_http_err, ip65_strerror(ip65_error),
                sizeof(g_http_err) - strlen(g_http_err) - 1);
    }
}

static int send_all(const uint8_t *p, uint16_t len)
{
    /* tcp_send already waits for ACK and closes on failure; do not retry. */
    while (len) {
        uint16_t n = len > TCP_MAX ? TCP_MAX : len;
        if (ui_aborted()) {
            return -1;
        }
        if (tcp_send(p, n)) {
            return -1;
        }
        p += n;
        len -= n;
        ip65_process();
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
        if (n > TCP_MAX) {
            n = TCP_MAX;
        }
        aux_read(off, bounce, n);
        aux_mainbank();
        if (ui_aborted()) {
            return -1;
        }
        if (tcp_send(bounce, n)) {
            return -1;
        }
        off += n;
        ip65_process();
    }
    return 0;
}

static int send_str(const char *s)
{
    return send_all((const uint8_t *)s, (uint16_t)strlen(s));
}

static int wait_done(void)
{
    rx_last = timer_read();
    while (!rx_eof) {
        if (ui_aborted()) {
            tcp_close();
            strcpy(g_http_err, "recv aborted");
            return -1;
        }
        /* ~20s with no TCP payload: Ollama hung or never closed. */
        if ((uint16_t)(timer_read() - rx_last) > 45000u) {
            tcp_close();
            strcpy(g_http_err, "recv timeout");
            return -1;
        }
        ip65_process();
    }
    tcp_close();
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
    {
        uint8_t tries;

        for (tries = 0; tries < 3; tries++) {
            aux_mainbank();
            if (tcp_connect(addr, port, on_tcp)) {
                send_fail("TCP connect");
                return -1;
            }
            ui_status("POST /api/chat ...");
            if (send_str(hdr) != 0) {
                tcp_close();
                if (tries == 2) {
                    send_fail("send hdr");
                    return -1;
                }
                continue;
            }
            if (send_aux(0, json_len) != 0) {
                tcp_close();
                if (tries == 2) {
                    send_fail("send aux");
                    return -1;
                }
                continue;
            }
            clock_reset_ms();
            break;
        }
    }
    if (wait_done() < 0) {
        return -1;
    }
    rc = finish_status();
    if (rc == 0) {
        strcpy(g_http_err, "HTTP 200");
    }
    return rc;
}

int http_probe_tags(uint32_t addr, uint16_t port)
{
    int rc;

    g_http_err[0] = 0;
    ui_status("TCP connect to Ollama...");
    rx_reset(0, 0, bod, 56);
    if (tcp_connect(addr, port, on_tcp)) {
        strcpy(g_http_err, "TCP connect failed");
        return -1;
    }
    sprintf(hdr,
            "GET /api/tags HTTP/1.0\r\n"
            "Host: %s:%u\r\n"
            "Connection: close\r\n"
            "\r\n",
            g_cfg.host, (unsigned)port);
    ui_status("GET /api/tags ...");
    if (send_str(hdr) < 0) {
        tcp_close();
        strcpy(g_http_err, "TCP up, send failed");
        return -1;
    }
    if (wait_done() < 0) {
        return -1;
    }
    rc = finish_status();
    if (rc < 0) {
        return rc;
    }
    if (g_cfg.model[0] && strstr(bod, g_cfg.model)) {
        strcpy(g_http_err, "HTTP 200, model listed");
        return 1;
    }
    strcpy(g_http_err, "HTTP 200, model not in tags");
    return 0;
}
