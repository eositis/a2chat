#include "a2chat.h"
#include <apple2.h>
#include <stdint.h>

/* W5100 mode register: (slot << 4) | 0xC084 */

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
    *mode = 0x80; /* software reset */
    *mode = 0x03; /* indirect + auto increment */
    *addr_hi = 0x00;
    *addr_lo = 0x01; /* GAR */
    *data = 0xA5;
    *data = 0x5A;
    *data = 0x00;
    *data = 0x01;
    *addr_hi = 0x00;
    *addr_lo = 0x01;
    a = *data;
    b = *data;
    *addr_hi = 0x00;
    *addr_lo = 0x01;
    *data = 0;
    *data = 0;
    *data = 0;
    *data = 0;
    *mode = saved;
    return (a == 0xA5 && b == 0x5A);
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
    if (ost >= APPLE_IIC && ost < APPLE_IIGS) {
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
