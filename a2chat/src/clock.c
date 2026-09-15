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

void clock_init(void)
{
    unsigned char h, m;

    g_p8 = 0;
    g_mf = 0;
    strcpy(g_wall, "--:--:--");
    if (*(volatile unsigned char *)0xFBB3 == 6 &&
        *(volatile unsigned char *)0xFBC0 == 0) {
        g_mf = mf_present();
    }
    if (p8_time(&h, &m)) {
        g_p8 = 1;
        put2(g_wall, h);
        g_wall[2] = ':';
        put2(g_wall + 3, m);
        g_wall[5] = ':';
        g_wall[6] = '0';
        g_wall[7] = '0';
        g_wall[8] = 0;
    } else if (g_mf && mf_timestr(g_wall)) {
        g_wall[8] = 0;
    }
#ifndef A2CHAT_HOST
    /* Clock-card IRQs are unsafe with cc65 LC. Mask them; IP65 is polled. */
    __asm__("cld");
    __asm__("sei");
#endif
}

uint8_t clock_kind(void)
{
    if (g_p8) {
        return CLOCK_NSC;
    }
    if (g_mf) {
        return CLOCK_MFMS;
    }
    return CLOCK_JIFFY;
}

const char *clock_kind_name(void)
{
    if (g_p8) {
        return "P8";
    }
    if (g_mf) {
        return "MFMS";
    }
    return "JIFFY";
}

void clock_wall(char *dst, unsigned dstsz)
{
    if (!dstsz) {
        return;
    }
    if (!g_wall[0]) {
        strcpy(g_wall, "--:--:--");
    }
    strncpy(dst, g_wall, dstsz - 1);
    dst[dstsz - 1] = 0;
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
