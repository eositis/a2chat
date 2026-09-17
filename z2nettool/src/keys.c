#include "a2net.h"

#define KBD (*(volatile unsigned char *)0xC000)
#define STROBE (*(volatile unsigned char *)0xC010)
#define OPENAPPLE (*(volatile unsigned char *)0xC061)

static unsigned char key_idle = 1;
static unsigned char key_last;

unsigned char key_oa(void)
{
    return (unsigned char)(OPENAPPLE & 0x80);
}

bool key_poll(unsigned char *out)
{
    unsigned char raw;
    unsigned char k;

    raw = KBD;
    if (!(raw & 0x80)) {
        key_idle = 1;
        return false;
    }

    /* Any write to $C010 clears the keyboard strobe. (void) reads can be
     * optimized away by cc65, which left the key latched and repeating. */
    STROBE = 0;

    k = (unsigned char)(raw & 0x7F);
    if (!key_idle && k == key_last)
        return false;
    key_idle = 0;
    key_last = k;
    *out = k;
    return true;
}
