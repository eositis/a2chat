#include "a2chat.h"
#include <stdio.h>
#include <string.h>
#ifndef A2CHAT_HOST
#include <ip65.h>
#endif

unsigned char p8_time(unsigned char *h, unsigned char *m);
unsigned char mf_present(void);
void mf_reset_ms(void);
uint32_t mf_get_ms(void);
unsigned char __fastcall__ mf_timestr(char *dst);

static uint8_t g_p8;
static uint8_t g_mf;
static uint16_t jiffy0;
static char g_wall[9];

static void put2(char *d, unsigned char v)
{
    unsigned char t = 0;

    while (v >= 10) {
        v = (unsigned char)(v - 10);
        t++;
    }
    d[0] = (char)('0' + t);
    d[1] = (char)('0' + v);
}

/* HH:MM:SS becomes HH:MM. "HH:MM AM" is left as the firmware wrote it. */
static void wall_hm(void)
{
    if (g_wall[2] == ':' && g_wall[5] == ':') {
        g_wall[5] = 0;
    }
}

void clock_init(void)
{
    unsigned char h, m;

    g_p8 = 0;
    g_mf = 0;
    strcpy(g_wall, "--:--");
    /* MegaFlash replaces the IIc/IIc+ ROM with a 4x or 5x image
     * ($FBBF = $04 or $05). $FBC0 == 0 is the original IIc and never
     * has MegaFlash. Skip IIe ($FBC0 = $E0): $C0C0 is W5100 there. */
    if (*(volatile unsigned char *)0xFBB3 == 6 &&
        *(volatile unsigned char *)0xFBC0 != 0xE0) {
        unsigned char rom = *(volatile unsigned char *)0xFBBF;
        if (rom == 4 || rom == 5) {
            g_mf = mf_present();
        }
    }
    if (g_mf && mf_timestr(g_wall)) {
        g_wall[8] = 0;
        wall_hm();
    } else if (p8_time(&h, &m)) {
        g_p8 = 1;
        put2(g_wall, h);
        g_wall[2] = ':';
        put2(g_wall + 3, m);
        g_wall[5] = 0;
    }
#ifndef A2CHAT_HOST
    /* Clock-card IRQs are unsafe with cc65 LC. Mask them; IP65 is polled. */
    __asm__("cld");
    __asm__("sei");
#endif
}

uint8_t clock_kind(void)
{
    if (g_mf) {
        return CLOCK_MFMS;
    }
    if (g_p8) {
        return CLOCK_NSC;
    }
    return CLOCK_JIFFY;
}

const char *clock_kind_name(void)
{
    if (g_mf) {
        return "MFMS";
    }
    if (g_p8) {
        return "P8";
    }
    return "JIFFY";
}

void clock_wall(char *dst, unsigned dstsz)
{
    unsigned char h, m;

    if (!dstsz) {
        return;
    }
    if (g_mf) {
        if (mf_timestr(g_wall)) {
            g_wall[8] = 0;
            wall_hm();
        }
    } else if (g_p8 && p8_time(&h, &m)) {
        put2(g_wall, h);
        g_wall[2] = ':';
        put2(g_wall + 3, m);
        g_wall[5] = 0;
    }
    if (!g_wall[0]) {
        strcpy(g_wall, "--:--");
    }
    strncpy(dst, g_wall, dstsz - 1);
    dst[dstsz - 1] = 0;
}

void clock_stamp(char *dst, unsigned dstsz)
{
    unsigned w;
    unsigned char day, mon, yr;

    if (dstsz < 18) {
        if (dstsz) {
            dst[0] = 0;
        }
        return;
    }
    clock_wall(dst + 9, dstsz - 9);
    w = (unsigned)*(volatile unsigned char *)0xBF90 |
        ((unsigned)*(volatile unsigned char *)0xBF91 << 8);
    day = (unsigned char)(w & 31);
    mon = (unsigned char)((w >> 5) & 15);
    yr = (unsigned char)((w >> 9) & 127);
    put2(dst, yr);
    dst[2] = '-';
    put2(dst + 3, mon);
    dst[5] = '-';
    put2(dst + 6, day);
    dst[8] = ' ';
}

void clock_reset_ms(void)
{
    jiffy0 = timer_read();
    if (g_mf) {
        mf_reset_ms();
    }
}

uint32_t clock_elapsed_ms(void)
{
    return (uint32_t)(uint16_t)(timer_read() - jiffy0);
}

uint32_t clock_post_ms(void)
{
    if (g_mf) {
        return mf_get_ms();
    }
    return clock_elapsed_ms();
}
