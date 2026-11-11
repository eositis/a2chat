/*
 * NIC overlay vector. One main binary loads A2CHAT.UTH or A2CHAT.COG into
 * the hole at nic_hole after probing which card is installed. The first
 * bytes of the overlay are this struct. cc65 does not pad it. src/nic_jump.s
 * and the overlay headers use these offsets: connect is 12, wall is 32.
 *
 * A later card is another file with the same struct. The main program
 * only learns the card's name by loading it and calling probe.
 */
#ifndef NIC_H
#define NIC_H

#include <stdint.h>

#define NIC_HOLE_SIZE 0x1780u

#define NIC_F_HOSTNAME 0x01u /* HOST= may be a name, not only dotted IPv4 */
#define NIC_F_IGNORE_IP 0x02u /* IP= GATEWAY= NETMASK= belong to the card */
#define NIC_F_CLOCK 0x04u /* wall() can read a clock */

struct nic {
    char magic[4]; /* 'N' 'I' 'C' '1' */
    uint8_t flags;
    uint8_t stream_default;
    uint16_t load_size; /* file bytes, patched after the link */
    uint8_t (*probe)(uint8_t slot);
    int (*init)(uint8_t slot);
    int (*connect)(uint32_t addr, const char *host, uint16_t port);
    int (*send)(const uint8_t *p, uint16_t len);
    uint8_t (*recv)(uint8_t *dst, uint8_t max);
    uint8_t (*connected)(void);
    void (*disconnect)(void);
    int (*localtime)(uint32_t *secs);
    uint8_t (*last_error)(void);
    const char *(*strerror)(uint8_t code);
    uint8_t (*should_retry)(void);
    void (*report)(void);
    void (*wall)(char *dst);
    const char *clock_name;
};

int nic_bringup(void);
int nic_connect(uint32_t addr, const char *host, uint16_t port);
int nic_send(const uint8_t *p, uint16_t len);
uint8_t nic_recv(uint8_t *dst, uint8_t max);
uint8_t nic_connected(void);
void nic_disconnect(void);
int nic_localtime(uint32_t *secs);
uint8_t nic_last_error(void);
const char *nic_strerror(uint8_t code);
uint8_t nic_should_retry(void);
void nic_report(void);
void nic_wall(char *dst);
uint8_t nic_flags(void);
uint8_t nic_stream_default(void);
uint8_t nic_has_clock(void);
const char *nic_clock_name(void);

uint8_t slot_find_cogs(uint8_t configured);
int slot_is_disk2(uint8_t slot);
uint8_t slot_prefer_uther(void);
/* 4 when this IIc has MegaFlash, else 0. Uthernet II shares that slot. */
uint8_t slot_megaflash(void);

#endif
