#ifndef A2NET_H
#define A2NET_H

#include <stdint.h>
#include <stdbool.h>
#include "ip65.h"

#define A2NET_VERSION "1.0"

#define COLS 40
#define ROWS 24
#define OUT_VIS 16
#define OUT_ROW0 7
#define OUT_LINES 32
#define NTOOLS 6
#define FIELD_MAX 39

#define TOOL_MENU 0
#define TOOL_PING 1
#define TOOL_TRACE 2
#define TOOL_NS 3
#define TOOL_WHOIS 4
#define TOOL_NC 5
#define TOOL_NMAP 6

#define NMAP_MAX_PORTS 32

#define FID_HOST 1
#define FID_PORT 2
#define FID_COUNT 3
#define FID_TIMEOUT 4
#define FID_MAXHOPS 5
#define FID_SERVER 6
#define FID_PROTO 7
#define FID_PAYLOAD 8
#define FID_PORTS 9

typedef struct {
    unsigned char id;
    unsigned char row;
    unsigned char maxlen;
    char *buf;
    const char *label;
    const char *help;
} field_t;

typedef struct {
    char host[FIELD_MAX + 1];
    char port[6];
    char count[4];
    char timeout[6];
    char max_hops[4];
    char server[FIELD_MAX + 1];
    char wserver[FIELD_MAX + 1];
    char proto[4];
    char payload[FIELD_MAX + 1];
    char ports[16];
} shared_t;

extern shared_t g_share;
extern unsigned char g_tool;
extern unsigned char g_focus;
extern unsigned char g_cur;
extern unsigned char g_nfields;
extern unsigned char g_in_output;
extern unsigned char g_busy;
extern field_t g_fields[6];
extern char g_nic_ok;
extern char g_dhcp_ok;
extern unsigned char g_eth_slot;
extern char g_init_err[40];

void ui_cls(void);
void ui_put(unsigned char row, unsigned char col, char c, unsigned char inv);
void ui_fill_row(unsigned char row, char c, unsigned char inv);
void ui_puts(unsigned char row, unsigned char col, const char *s, unsigned char inv);
void ui_draw_hotkeys(void);
void ui_draw_sep(void);
void ui_draw_help(const char *s);
void ui_draw_fields(void);
void ui_draw_output(void);
void ui_draw_menu(void);
void ui_draw_tool(void);
void ui_log(const char *s);
void ui_logf_ip(const char *prefix, uint32_t ip);
void ui_handle_key(unsigned char k);
void ui_setup_tool(unsigned char tool);
void ui_scroll(int dir);

unsigned char key_oa(void);
bool key_poll(unsigned char *out);

void net_init(void);
uint32_t net_resolve(const char *host);
uint16_t parse_u16(const char *s);
void fmt_u16(char *dst, uint16_t v);
void fmt_ip_line(char *dst, const char *label, uint32_t ip);
void fmt_nic_line(char *dst);

void ping_run(void);
void traceroute_run(void);
void nslookup_run(void);
void whois_run(void);
void nc_run(void);
void nmap_run(void);

extern uint32_t icmp_trace_dest;
extern uint32_t icmp_trace_from;
extern uint16_t icmp_trace_ms;
extern uint8_t icmp_trace_ttl;
uint8_t __fastcall__ icmp_trace_hop(uint16_t wait_ms);

#endif
