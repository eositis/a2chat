#include <string.h>
#include "a2net.h"

char g_nic_ok;
char g_dhcp_ok;
unsigned char g_eth_slot;
char g_init_err[40];

/* Slot 4 first: MegaFlash storage + Uthernet II share that slot
 * ($C0C0-$C0C3 storage, $C0C4-$C0C7 W5100). Do not skip ROM slots.
 * Slot 6 (Disk II) is last so a probe write to $C0E4 is avoided if possible. */
static const unsigned char scan_slots[] = { 4, 3, 7, 2, 1, 5, 6 };

void net_init(void)
{
    unsigned char i;
    char line[40];

    g_nic_ok = 0;
    g_dhcp_ok = 0;
    g_eth_slot = 0;
    g_init_err[0] = 0;

    ui_cls();
    ui_puts(0, 0, "A2NETTOOL  scanning NIC", 0);
    ui_puts(2, 0, "Tries every slot (ROM ok)", 0);
    ui_puts(3, 0, "MegaFlash NIC is co-located", 0);

    for (i = 0; i < sizeof(scan_slots); i++) {
        unsigned char s = scan_slots[i];
        line[0] = 'S';
        line[1] = 'L';
        line[2] = 'O';
        line[3] = 'T';
        line[4] = ' ';
        line[5] = (char)('0' + s);
        line[6] = '.';
        line[7] = '.';
        line[8] = '.';
        line[9] = 0;
        ui_fill_row(5, ' ', 0);
        ui_puts(5, 0, line, 0);

        if (!ip65_init(s)) {
            g_nic_ok = 1;
            g_eth_slot = s;
            break;
        }
    }

    if (!g_nic_ok) {
        strcpy(g_init_err, ip65_strerror(ip65_error));
        return;
    }

    ui_puts(7, 0, "DHCP...", 0);
    if (dhcp_init()) {
        strcpy(g_init_err, ip65_strerror(ip65_error));
        return;
    }
    g_dhcp_ok = 1;
}

uint32_t net_resolve(const char *host)
{
    uint32_t ip;
    if (!host || !host[0])
        return 0;
    ip = dns_resolve(host);
    return ip;
}

uint16_t parse_u16(const char *s)
{
    uint16_t v = 0;
    if (!s)
        return 0;
    while (*s == ' ')
        s++;
    while (*s >= '0' && *s <= '9') {
        v = (uint16_t)(v * 10 + (*s - '0'));
        s++;
    }
    return v;
}

void fmt_u16(char *dst, uint16_t v)
{
    char tmp[6];
    unsigned char n = 0;
    unsigned char i;
    if (v == 0) {
        dst[0] = '0';
        dst[1] = 0;
        return;
    }
    while (v && n < 5) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    i = 0;
    while (n)
        dst[i++] = tmp[--n];
    dst[i] = 0;
}

void fmt_ip_line(char *dst, const char *label, uint32_t ip)
{
    unsigned char i = 0;
    const char *p = label;
    const char *dq;
    while (*p && i < 39) {
        dst[i++] = *p++;
    }
    dq = dotted_quad(ip);
    while (*dq && i < 39) {
        dst[i++] = *dq++;
    }
    dst[i] = 0;
}

void fmt_nic_line(char *dst)
{
    unsigned char i = 0;
    const char *p = eth_name;
    while (*p && i < 28)
        dst[i++] = *p++;
    dst[i++] = ' ';
    dst[i++] = 'S';
    dst[i++] = (char)('0' + g_eth_slot);
    dst[i] = 0;
}
