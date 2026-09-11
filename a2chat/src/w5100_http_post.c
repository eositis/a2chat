/******************************************************************************
 * HTTP/1.0 POST for W5100 hardware TCP — modeled on ip65 apps/w5100_http.c
 ******************************************************************************/

#pragma optimize      (on)
#pragma static-locals (on)

#include "a2chat.h"
#include "w5100.h"
#include <ip65.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static int send_bytes(const char *p, uint16_t len)
{
    register volatile uint8_t *data = w5100_data;

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
        if (len < snd) {
            snd = len;
        }
        for (i = 0; i < snd; ++i) {
            *data = *p++;
        }
        w5100_send_commit(snd);
        len -= snd;
    }
    return 0;
}

static int send_str(const char *s)
{
    return send_bytes(s, (uint16_t)strlen(s));
}

static int recv_some(char *buf, uint16_t max, uint16_t *got)
{
    register volatile uint8_t *data = w5100_data;
    uint16_t rcv;
    uint16_t i;

    *got = 0;
    if (ui_aborted()) {
        w5100_disconnect();
        return -1;
    }
    rcv = w5100_receive_request();
    if (!rcv) {
        if (!w5100_connected()) {
            return 0;
        }
        return 1; /* try again */
    }
    if (rcv > max) {
        rcv = max;
    }
    for (i = 0; i < rcv; ++i) {
        buf[i] = *data;
    }
    w5100_receive_commit(rcv);
    *got = rcv;
    return 1;
}

int http_post_file(uint32_t addr, uint16_t port, const char *url_path,
                   const char *body_path,
                   void (*on_byte)(char ch, void *user), void *user)
{
    FILE *bf;
    long clen;
    char hdr[160];
    char buf[128];
    uint16_t got;
    uint16_t hlen = 0;
    uint8_t body = 0;
    uint8_t chunked = 0;
    uint16_t chunk_left = 0;
    uint8_t chunk_st = 0; /* 0 size hex, 1 data, 2 crlf after data */
    char hex[8];
    uint8_t hexn = 0;
    int rc;

    bf = fopen(body_path, "rb");
    if (!bf) {
        strcpy(g_http_err, "cannot open POST body");
        return -1;
    }
    fseek(bf, 0, SEEK_END);
    clen = ftell(bf);
    fseek(bf, 0, SEEK_SET);

    if (!w5100_connect_addr(addr, port)) {
        fclose(bf);
        strcpy(g_http_err, "TCP connect failed");
        return -1;
    }

    sprintf(hdr,
            "POST %s HTTP/1.0\r\n"
            "Host: %s:%u\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n",
            url_path, g_cfg.host, (unsigned)port, clen);
    if (send_str(hdr) < 0) {
        fclose(bf);
        strcpy(g_http_err, "send headers failed");
        return -1;
    }
    while ((got = (uint16_t)fread(buf, 1, sizeof(buf), bf)) > 0) {
        if (send_bytes(buf, got) < 0) {
            fclose(bf);
            strcpy(g_http_err, "send body failed");
            return -1;
        }
    }
    fclose(bf);

    /* response headers then body */
    hlen = 0;
    for (;;) {
        rc = recv_some(buf, sizeof(buf), &got);
        if (rc < 0) {
            strcpy(g_http_err, "recv aborted");
            return -1;
        }
        if (rc == 0 && !body) {
            w5100_disconnect();
            strcpy(g_http_err, "no HTTP response");
            return -1;
        }
        if (got == 0) {
            if (body) {
                break;
            }
            continue;
        }
        {
            uint16_t i;
            for (i = 0; i < got; ++i) {
                char ch = buf[i];
                if (!body) {
                    if (hlen < sizeof(hdr) - 1) {
                        hdr[hlen++] = ch;
                        hdr[hlen] = 0;
                    }
                    if (hlen >= 4 && memcmp(hdr + hlen - 4, "\r\n\r\n", 4) == 0) {
                        hdr[7] = '0';
                        if (memcmp(hdr, "HTTP/1.0 200", 12) &&
                            memcmp(hdr, "HTTP/1.0 201", 12)) {
                            w5100_disconnect();
                            strncpy(g_http_err, hdr, 40);
                            g_http_err[40] = 0;
                            return -2;
                        }
                        if (strstr(hdr, "chunked") || strstr(hdr, "Chunked")) {
                            chunked = 1;
                        }
                        body = 1;
                        if (!chunked) {
                            /* remaining bytes in this buffer after header */
                            continue;
                        }
                        continue;
                    }
                } else if (!chunked) {
                    on_byte(ch, user);
                } else {
                    if (chunk_st == 0) {
                        if (ch == '\r') {
                            continue;
                        }
                        if (ch == '\n') {
                            uint16_t v = 0;
                            uint8_t k;
                            hex[hexn] = 0;
                            for (k = 0; k < hexn; k++) {
                                char h = (char)tolower((unsigned char)hex[k]);
                                v <<= 4;
                                if (h >= '0' && h <= '9') {
                                    v |= (uint16_t)(h - '0');
                                } else if (h >= 'a' && h <= 'f') {
                                    v |= (uint16_t)(h - 'a' + 10);
                                }
                            }
                            hexn = 0;
                            if (v == 0) {
                                w5100_disconnect();
                                return 0;
                            }
                            chunk_left = v;
                            chunk_st = 1;
                        } else if (hexn < 7 && isxdigit((unsigned char)ch)) {
                            hex[hexn++] = ch;
                        }
                    } else if (chunk_st == 1) {
                        on_byte(ch, user);
                        chunk_left--;
                        if (chunk_left == 0) {
                            chunk_st = 2;
                        }
                    } else {
                        if (ch == '\n') {
                            chunk_st = 0;
                        }
                    }
                }
            }
        }
        if (rc == 0) {
            break;
        }
    }
    w5100_disconnect();
    strcpy(g_http_err, "HTTP 200");
    return 0;
}

int http_probe_tags(uint32_t addr, uint16_t port)
{
    static char hdr[144];
    static char buf[64];
    static char body[160];
    uint16_t got;
    uint16_t hlen;
    uint16_t blen;
    uint8_t have_hdr;
    int rc;
    uint16_t i;

    g_http_err[0] = 0;
    ui_status("TCP connect to Ollama...");
    if (!w5100_connect_addr(addr, port)) {
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
        strcpy(g_http_err, "TCP up, send failed");
        w5100_disconnect();
        return -1;
    }
    hlen = 0;
    blen = 0;
    have_hdr = 0;
    hdr[0] = 0;
    for (;;) {
        rc = recv_some(buf, sizeof(buf), &got);
        if (rc < 0) {
            strcpy(g_http_err, "TCP up, recv aborted");
            return -1;
        }
        if (rc == 0 && !have_hdr) {
            w5100_disconnect();
            strcpy(g_http_err, "TCP up, no HTTP reply");
            return -1;
        }
        if (got == 0) {
            if (have_hdr) {
                break;
            }
            continue;
        }
        for (i = 0; i < got; ++i) {
            char ch = buf[i];
            if (!have_hdr) {
                if (hlen < sizeof(hdr) - 1) {
                    hdr[hlen++] = ch;
                    hdr[hlen] = 0;
                }
                if (hlen >= 4 && memcmp(hdr + hlen - 4, "\r\n\r\n", 4) == 0) {
                    have_hdr = 1;
                }
            } else if (blen < sizeof(body) - 1) {
                body[blen++] = ch;
                body[blen] = 0;
            }
        }
        if (rc == 0) {
            break;
        }
        if (have_hdr && blen >= sizeof(body) - 1) {
            break;
        }
    }
    w5100_disconnect();
    if (hlen >= 12) {
        hdr[7] = '0';
    }
    if (hlen < 12 || memcmp(hdr, "HTTP/1.0 200", 12)) {
        strncpy(g_http_err, hdr, 40);
        g_http_err[40] = 0;
        return -2;
    }
    if (g_cfg.model[0] && strstr(body, g_cfg.model)) {
        strcpy(g_http_err, "HTTP 200, model listed");
        return 1;
    }
    strcpy(g_http_err, "HTTP 200, model not in tags");
    return 0;
}
