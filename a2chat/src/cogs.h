/*
 * CoGS network transport for A2CHAT.
 *
 * Built with -DA2CHAT_COGS. src/cogs.c implements the same w5100_* API
 * that src/w5100.c provides for the Uthernet II (see w5100.h), so the rest
 * of A2CHAT barely notices which card it is talking to. The differences:
 *
 *  - The card does DHCP / static addressing, DNS and TCP itself. IP65 is
 *    not initialized and no MACRAW traffic ever happens.
 *  - Received bytes are popped from the card's DATA register, so after
 *    w5100_receive_request() the caller reads them with cogs_rx() (the
 *    W5100 "read *w5100_data n times" idiom still works for reads, but
 *    A2CHAT uses cogs_rx()).
 *  - Sends are whole SEND frames whose length is known up front, so
 *    callers use cogs_send() instead of w5100_send_request()/commit().
 *
 * Register-level primitives live in src/cogs_io.s.
 */
#ifndef _COGS_H_
#define _COGS_H_

#include <stdint.h>

/* src/cogs_io.s */
void __fastcall__ cogs_set_slot(uint8_t slot);
uint8_t __fastcall__ cogs_probe(uint8_t slot);
uint8_t __fastcall__ cogs_reg_rd(uint8_t reg);
void __fastcall__ cogs_reg_wr(uint8_t reg, uint8_t val);
uint16_t cogs_avail(void);
int cogs_get(void);
uint8_t __fastcall__ cogs_getn(uint8_t *dst, uint8_t n);
uint8_t __fastcall__ cogs_skip(uint16_t n);
uint8_t __fastcall__ cogs_put(uint8_t b);
uint8_t __fastcall__ cogs_putn(const uint8_t *src, uint16_t n);
uint8_t cogs_resync(void);
uint8_t cogs_irq_off(void);

/* src/cogs.c */

/* Pop n (<= 255) bytes of the pending DATA payload into dst, after
 * w5100_receive_request(). Replaces w5100_receive_commit(). Return 0 on
 * success, nonzero if the card stalled (the socket is then closed). */
uint8_t cogs_rx(uint8_t *dst, uint8_t n);

/* Send len bytes on the open TCP socket (split into SEND frames).
 * Return 0 on success, -1 if the socket is gone or the user aborted. */
int cogs_send(const uint8_t *p, uint16_t len);

/* NETINFO: link_state 0 down, 1 joining, 2 up. ip in network byte order
 * (same layout as IP65's cfg_ip). Return 0 on success. */
int cogs_netinfo(uint8_t *link_state, uint32_t *ip);

/* Card result code of the last failure (CONNECT, SEND_ACK, ERROR event).
 * 2 DNS_FAIL, 3 CONN_TIMEOUT, 4 CONN_REFUSED, 7 NO_FREE_SOCK, ... */
uint8_t cogs_last_error(void);

/* Short name for a card error code. */
const char *cogs_strerror(uint8_t code);

/* Print card errors, resyncs and dropped bytes seen since the last
 * connect, if there were any. */
void cogs_report(void);

#endif
