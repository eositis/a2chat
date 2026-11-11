#include "a2chat.h"
#include <apple2.h>
#include <stdint.h>
#ifdef A2CHAT_COGS
#include "cogs.h"
#endif

/* W5100 mode register: (slot << 4) | 0xC084 */

#ifdef A2CHAT_COGS

/* A Disk II controller's slot ROM: $Cn01=$20 $Cn03=$00 $Cn05=$03 $CnFF=$00.
 * Its $C0nX switches move the drive head, so never probe that slot. */
static int is_disk2(uint8_t s)
{
    const volatile uint8_t *rom = (const uint8_t *)(0xC000 | (s << 8));
    return rom[1] == 0x20 && rom[3] == 0x00 && rom[5] == 0x03 &&
           rom[0xFF] == 0x00;
}

/* SLOT= wins. Otherwise look for the CoGS ID register ($C0n5 = $A2),
 * high slots first. 0 means not found. */
uint8_t slot_resolve(uint8_t configured)
{
    uint8_t s;

    if (configured >= 1 && configured <= 7) {
        return configured;
    }
    for (s = 7; s >= 1; s--) {
        if (!is_disk2(s) && cogs_probe(s)) {
            return s;
        }
    }
    return 0;
}

#else /* A2CHAT_COGS */

static int w5100_present(uint8_t slot)
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
    return (a == 0x07 && b == 0xD0);
}

uint8_t slot_resolve(uint8_t configured)
{
    uint8_t prefer;
    uint8_t s;
    unsigned ost;

    if (configured >= 1 && configured <= 7) {
        return configured;
    }
    ost = get_ostype();
    /* IIc MegaFlash and a IIgs Uthernet II are both usually slot 4.
     * An enhanced IIe still tries slot 3 first. */
    if (ost == APPLE_IIGS || (ost >= APPLE_IIC && ost < APPLE_IIGS)) {
        prefer = 4;
    } else {
        prefer = 3;
    }
    if (w5100_present(prefer)) {
        return prefer;
    }
    for (s = 1; s <= 7; s++) {
        if (s == prefer) {
            continue;
        }
        if (w5100_present(s)) {
            return s;
        }
    }
    return prefer; /* last resort: wget65 default-ish */
}

#endif /* A2CHAT_COGS */
