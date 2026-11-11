/******************************************************************************
 * HTTP/1.0 over the NIC overlay loaded at startup. The driver owns the
 * socket. Do not call ip65_process from here.
 ******************************************************************************/

#pragma static-locals (on)
#pragma optimize (on)

#include "a2chat.h"
#include "nic.h"
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
static uint8_t rx_closed;
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

/* One window from the loaded driver. The driver commits or pops what it
 * copies, so a short read leaves the rest for the next call. */
static uint8_t pump_rx(void)
{
    uint8_t n;
    uint8_t i;

    n = nic_recv((uint8_t *)rxwin, (uint8_t)sizeof(rxwin));
    if (!n) {
        if (nic_connected()) {
            rx_closed = 0;
            return 0;
        }
        /* llama.cpp sends the reply and closes before the next poll.
         * Sn_CR can still be busy, so the first size read is 0. */
        if (rx_closed < 200) {
            rx_closed++;
            return 0;
        }
        rx_eof = 1;
        return 0;
    }
    rx_closed = 0;
    i = 0;
    while (i < n && !rx_have_hdr) {
        feed_byte(rxwin[i]);
        i++;
    }
    if (i < n) {
        if (rx_chunked || !rx_on_bytes) {
            while (i < n) {
                feed_byte(rxwin[i]);
                i++;
            }
        } else {
            rx_on_bytes(rxwin + i, (unsigned)(n - i), rx_user);
        }
    }
    return 1;
}

static void rx_reset(void (*on_bytes)(const char *p, unsigned n, void *user),
                     void *user, char *accum, uint16_t accum_max)
{
    rx_eof = 0;
    rx_closed = 0;
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
    return nic_send(p, len);
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
            nic_disconnect();
            strcpy(g_http_err, "recv aborted");
            return -1;
        }
        if (pump_rx()) {
            rx_last = timer_jiffy();
        } else if ((uint16_t)(timer_jiffy() - rx_last) > 45000u) {
            /* ~45s of vertical blanks with no TCP payload. */
            nic_disconnect();
            strcpy(g_http_err, "recv timeout");
            return -1;
        }
    }
    nic_disconnect();
    return 0;
}

static int finish_status(void)
{
    int st;

    if (!rx_have_hdr) {
        /* Status line arrived, but the blank line that ends the headers
         * was still in the W5100 buffer when the peer closed. */
        if (rx_hdr[0] == 'H' && rx_hdr[1] == 'T' &&
            rx_hdr[2] == 'T' && rx_hdr[3] == 'P') {
            rx_have_hdr = 1;
        } else {
            fmt_start(g_http_err, sizeof(g_http_err));
            fmt_s("no HTTP ");
            fmt_u(rx_hlen);
            fmt_end();
            return -1;
        }
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

    fmt_start(hdr, 160);
    fmt_s("POST ");
    fmt_s(url_path);
    fmt_s(" HTTP/1.0\r\nHost: ");
    fmt_s(g_cfg.host);
    fmt_c(':');
    fmt_u(port);
    fmt_s("\r\nContent-Type: application/json\r\nContent-Length: ");
    fmt_u(json_len);
    fmt_s("\r\nConnection: close\r\n\r\n");
    fmt_end();
    hlen = (uint16_t)fmt_len();
    if (hlen >= 159) {
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
        nic_disconnect();
        strcpy(g_http_err, "send hdr");
        return -1;
    }
    if (send_aux(0, json_len) != 0) {
        nic_disconnect();
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

/* One try is about 30 seconds on the W5100. Two retries after that.
 * The loaded driver can refuse a retry (DNS failure, connection refused). */
static int connect_30(uint32_t addr, uint16_t port)
{
    uint8_t try;
    uint8_t code;
    const char *why;

    for (try = 0; try < 3; try++) {
        if (ui_aborted()) {
            strcpy(g_http_err, "recv aborted");
            return -1;
        }
        ui_status(try ? "Retry connect..." : "Connecting...");
        if (nic_connect(addr, g_cfg.host, port) == 0) {
            return 0;
        }
        nic_disconnect();
        if (!nic_should_retry()) {
            break;
        }
    }
    code = nic_last_error();
    why = code ? nic_strerror(code) : 0;
    if (why && why[0]) {
        fmt_start(g_http_err, sizeof(g_http_err));
        fmt_s("TCP: ");
        fmt_s(why);
        fmt_end();
        return -1;
    }
    strcpy(g_http_err, "TCP connect failed");
    return -1;
}

static int http_get(uint32_t addr, uint16_t port, const char *path)
{
    g_http_err[0] = 0;
    rx_reset(0, 0, 0, 0);
    if (connect_30(addr, port)) {
        return -1;
    }
    fmt_start(hdr, 160);
    fmt_s("GET ");
    fmt_s(path);
    fmt_s(" HTTP/1.0\r\nHost: ");
    fmt_s(g_cfg.host);
    fmt_c(':');
    fmt_u(port);
    fmt_s("\r\nConnection: close\r\n\r\n");
    fmt_end();
    if (fmt_len() >= 159) {
        nic_disconnect();
        strcpy(g_http_err, "headers too long");
        return -1;
    }
    ui_status(path);
    if (send_str(hdr) < 0) {
        nic_disconnect();
        strcpy(g_http_err, "TCP up, send failed");
        return -1;
    }
    if (wait_done() < 0) {
        return -1;
    }
    return finish_status();
}

static int http_missing(void)
{
    const char *p = g_http_err;

    while (*p) {
        if (p[0] == '4' && p[1] == '0' && p[2] == '4') {
            return 1;
        }
        p++;
    }
    return 0;
}

/* OpenAI first. A 404 means this server does not have that path, so try
 * Ollama. A TCP failure is the same for both and is not retried. */
int api_probe(uint32_t addr, uint16_t port)
{
    int rc;

    if (g_cfg.api == A2CHAT_API_OLLAMA || g_cfg.api == A2CHAT_API_OLLA) {
        g_api_lock = g_cfg.api;
        return http_get(addr, port, cfg_api_path(&g_cfg, "/api/tags"));
    }
    if (g_cfg.api == A2CHAT_API_OPENAI) {
        g_api_lock = A2CHAT_API_OPENAI;
        return http_get(addr, port, cfg_api_path(&g_cfg, "/api/tags"));
    }
    if (g_api_lock) {
        return http_get(addr, port, cfg_api_path(&g_cfg, "/api/tags"));
    }
    rc = http_get(addr, port, "/v1/models");
    if (rc == 0) {
        g_api_lock = A2CHAT_API_OPENAI;
        ui_print("API OpenAI");
        ui_nl();
        return 0;
    }
    if (rc != -2 || !http_missing()) {
        return rc;
    }
    ui_status("Trying Ollama...");
    rc = http_get(addr, port, "/api/tags");
    if (rc == 0) {
        g_api_lock = A2CHAT_API_OLLAMA;
        ui_print("API Ollama");
        ui_nl();
    }
    return rc;
}

int api_prepare(uint32_t addr, uint16_t port)
{
    if (g_cfg.api != A2CHAT_API_AUTO) {
        g_api_lock = g_cfg.api;
        return 0;
    }
    if (g_api_lock) {
        return 0;
    }
    return api_probe(addr, port);
}
