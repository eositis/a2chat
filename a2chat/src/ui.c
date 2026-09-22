#include "a2chat.h"
#include <apple2enh.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <apple2.h>
#include <ip65.h>

#define CHROME_TOP 0
#define CHAT_TOP 1
#define CHAT_BOT 22
#define HELP_ROW 23
#define PANE 22

#define SB_LINES 128
#define SB_ADDR 0x0800u

#define STORE80_ON  (*(volatile unsigned char *)0xC001)
#define PAGE2_OFF   (*(volatile unsigned char *)0xC054)
#define PAGE2_ON    (*(volatile unsigned char *)0xC055)

static uint8_t chat_y = CHAT_TOP;
static uint8_t chat_x = 0;
static char outbuf[16];
static uint8_t outn;
static uint8_t have_perf;
static uint8_t sb_head;
static uint8_t sb_count;
static uint8_t sb_view;

uint8_t g_slot;
char g_status[81];
char g_io80[80];

void ui_clear_chat(void);

void __fastcall__ chat_copy_row(uint8_t dst, uint8_t src);
void __fastcall__ chat_pack_row(uint8_t row, unsigned char *dst);
void __fastcall__ chat_unpack_row(uint8_t row, const unsigned char *src);

static unsigned text_base(uint8_t row)
{
    return 0x400u + (unsigned)((row & 7) * 0x80 + (row >> 3) * 40);
}

static unsigned char glyph(char ch, uint8_t inverse)
{
    unsigned char c = (unsigned char)ch & 0x7f;

    if (c < 32) {
        c = ' ';
    }
    if (inverse) {
        if (c >= 'a' && c <= 'z') {
            c = (unsigned char)(c - 32);
        }
        return (unsigned char)(c & 0x3f);
    }
    return (unsigned char)(c | 0x80);
}

static void chat_blit(uint8_t col, uint8_t row, const char *s, uint8_t n,
                      uint8_t inverse)
{
    unsigned base;
    uint8_t i;

    if (!n) {
        return;
    }
    base = text_base(row);
    STORE80_ON = 0;
    /* One aux pass (even cols), one main pass (odd cols). */
    PAGE2_ON = 0;
    for (i = 0; i < n; i++) {
        if (((col + i) & 1) == 0) {
            *(volatile unsigned char *)(unsigned)(base + ((col + i) >> 1)) =
                glyph(s[i], inverse);
        }
    }
    PAGE2_OFF = 0;
    for (i = 0; i < n; i++) {
        if ((col + i) & 1) {
            *(volatile unsigned char *)(unsigned)(base + ((col + i) >> 1)) =
                glyph(s[i], inverse);
        }
    }
}

static int open_apple_down(void)
{
    return (*(volatile unsigned char *)0xC061) & 0x80;
}

int ui_aborted(void)
{
    unsigned char k;
    if (input_check_for_abort_key()) {
        return 1;
    }
    k = *(volatile unsigned char *)0xC000;
    if (k & 0x80) {
        k &= 0x7f;
        if (k == '.' && open_apple_down()) {
            *(volatile unsigned char *)0xC010 = 0;
            return 1;
        }
    }
    return 0;
}

static void chat_clear_line(uint8_t row)
{
    unsigned addr = text_base(row);
    unsigned i;

    STORE80_ON = 0;
    PAGE2_OFF = 0;
    for (i = 0; i < 40; i++) {
        *(volatile unsigned char *)(addr + i) = 0xA0;
    }
    PAGE2_ON = 0;
    for (i = 0; i < 40; i++) {
        *(volatile unsigned char *)(addr + i) = 0xA0;
    }
    PAGE2_OFF = 0;
}

static void chat_copy_row_up(void)
{
    uint8_t r;

    for (r = CHAT_TOP; r < CHAT_BOT; r++) {
        chat_copy_row(r, (uint8_t)(r + 1));
    }
    chat_clear_line(CHAT_BOT);
}

static unsigned sb_line_addr(uint8_t idx)
{
    return (unsigned)(SB_ADDR + (unsigned)idx * 80u);
}

static void sb_store(uint8_t idx, const unsigned char *src)
{
    aux_abs_write(sb_line_addr(idx), src, 80);
    aux_mainbank();
}

static void sb_load(uint8_t idx, unsigned char *dst)
{
    aux_abs_read(sb_line_addr(idx), dst, 80);
    aux_mainbank();
}

static void sb_push_row(uint8_t row)
{
    chat_pack_row(row, (unsigned char *)g_io80);
    sb_store(sb_head, (unsigned char *)g_io80);
    sb_head++;
    if (sb_head >= SB_LINES) {
        sb_head = 0;
    }
    if (sb_count < SB_LINES) {
        sb_count++;
    }
}

static uint8_t sb_idx_from_oldest(uint8_t n)
{
    uint8_t oldest;

    if (sb_count < SB_LINES) {
        oldest = 0;
    } else {
        oldest = sb_head;
    }
    return (uint8_t)(((unsigned)oldest + (unsigned)n) % SB_LINES);
}

static void sb_paint(void)
{
    uint8_t i;
    unsigned start;
    unsigned logi;

    if (sb_count > PANE + sb_view) {
        start = (unsigned)sb_count - (unsigned)PANE - (unsigned)sb_view;
    } else {
        start = 0;
    }
    for (i = 0; i < PANE; i++) {
        logi = start + i;
        if (logi >= sb_count) {
            chat_clear_line((uint8_t)(CHAT_TOP + i));
        } else {
            sb_load(sb_idx_from_oldest((uint8_t)logi), (unsigned char *)g_io80);
            chat_unpack_row((uint8_t)(CHAT_TOP + i), (unsigned char *)g_io80);
        }
    }
}

static void sb_show_tail(void)
{
    sb_view = 0;
    sb_paint();
    if (sb_count >= PANE) {
        chat_copy_row_up();
        chat_y = CHAT_BOT;
    } else {
        chat_y = (uint8_t)(CHAT_TOP + sb_count);
    }
    chat_x = 0;
    chat_clear_line(chat_y);
}

static void sb_pin_tail(void)
{
    if (!sb_view) {
        return;
    }
    sb_show_tail();
}

static void sb_page(int dir)
{
    unsigned max_off;

    if (dir < 0) {
        if (!sb_count) {
            return;
        }
        max_off = sb_count > PANE ? (unsigned)(sb_count - PANE) : 0;
        if (sb_view < 255) {
            sb_view++;
        }
        if ((unsigned)sb_view > max_off) {
            sb_view = (uint8_t)max_off;
        }
        sb_paint();
        return;
    }
    if (!sb_view) {
        return;
    }
    sb_view--;
    if (!sb_view) {
        sb_show_tail();
    } else {
        sb_paint();
    }
}

static void chat_row_done(void)
{
    sb_pin_tail();
    sb_push_row(chat_y);
    if (chat_y < CHAT_BOT) {
        chat_y++;
    } else {
        chat_copy_row_up();
    }
    chat_x = 0;
    chat_clear_line(chat_y);
}

void ui_set_perf(const char *s)
{
    have_perf = 0;
    if (!s || !s[0]) {
        return;
    }
    strncpy(g_status, s, 80);
    g_status[80] = 0;
    have_perf = 1;
}

static void help_row(void)
{
    const char *h = "/config /ping /cat /new /model /quit /about";
    char t[10];
    unsigned char col;
    unsigned n;

    clock_wall(t, sizeof t);
    n = (unsigned)strlen(t) + 2u + (unsigned)strlen(A2CHAT_BUILD_STR);
    col = (unsigned char)(80u - n);
    gotoxy(0, HELP_ROW);
    revers(1);
    cputs(h);
    if ((unsigned)strlen(h) < col) {
        cclear((unsigned char)(col - (unsigned)strlen(h)));
    }
    gotoxy(col, HELP_ROW);
    revers(1);
    cputs(t);
    cputc(' ');
    cputc('B');
    cputs(A2CHAT_BUILD_STR);
    revers(0);
}

void ui_redraw_chrome(void)
{
    char line[81];

#ifndef A2CHAT_HOST
    __asm__("cld");
    __asm__("sei");
#endif

    if (have_perf) {
        sprintf(line, "A2CHAT  %s:%u  %-10s %s",
                g_cfg.host, (unsigned)g_cfg.port, g_cfg.model, g_status);
    } else {
        sprintf(line, "A2CHAT  %s:%u  %-12s  Slot%u  [%s]",
                g_cfg.host, (unsigned)g_cfg.port, g_cfg.model,
                (unsigned)g_slot, g_net_ok ? "Conn" : "----");
    }
    line[80] = 0;
    gotoxy(0, CHROME_TOP);
    revers(1);
    cputs(line);
    cclear((unsigned char)(80 - strlen(line)));
    revers(0);
    help_row();
}

void ui_init(void)
{
    videomode(VIDEOMODE_80COL);
#ifndef A2CHAT_HOST
    /* videomode/PR#3 CLI; clock IRQ SED then ROM COUT dies at $FDF7. */
    __asm__("cld");
    __asm__("sei");
#endif
    clrscr();
    cursor(1);
    chat_y = CHAT_TOP;
    chat_x = 0;
    outn = 0;
    ui_redraw_chrome();
    ui_clear_chat();
}

void ui_clear_chat(void)
{
    uint8_t r;

#ifndef A2CHAT_HOST
    __asm__("sei");
#endif

    outn = 0;
    chat_x = 0;
    chat_y = CHAT_TOP;
    sb_head = 0;
    sb_count = 0;
    sb_view = 0;
    for (r = CHAT_TOP; r <= CHAT_BOT; r++) {
        chat_clear_line(r);
    }
}

void ui_status(const char *msg)
{
    strncpy(g_status, msg, 80);
    g_status[80] = 0;
    gotoxy(0, CHROME_TOP);
    revers(1);
    cputs(g_status);
    cclear((unsigned char)(80 - strlen(g_status)));
    revers(0);
}

void ui_flush(void)
{
    sb_pin_tail();
    while (outn) {
        uint8_t room = (uint8_t)(80 - chat_x);
        uint8_t n;

        if (room == 0) {
            chat_row_done();
            room = 80;
        }
        n = outn < room ? outn : room;
        chat_blit(chat_x, chat_y, outbuf, n, 0);
        chat_x = (uint8_t)(chat_x + n);
        outn = (uint8_t)(outn - n);
        if (outn) {
            memmove(outbuf, outbuf + n, outn);
        }
        if (chat_x >= 80) {
            chat_row_done();
        }
    }
}

void ui_nl(void)
{
    ui_flush();
    chat_row_done();
}

void ui_label(const char *s)
{
    uint8_t n;

    ui_flush();
    if (chat_x != 0) {
        ui_nl();
    }
    n = (uint8_t)strlen(s);
    if (n) {
        chat_blit(0, chat_y, s, n, 1);
        chat_x = n;
    }
    outbuf[0] = ' ';
    chat_blit(chat_x, chat_y, outbuf, 1, 0);
    chat_x++;
}

void ui_print_ch(char ch)
{
    if (ch == '\n' || ch == '\r') {
        ui_flush();
        ui_nl();
        return;
    }
    if (ch == '\t') {
        ch = ' ';
    }
    if (chat_x + outn >= 80) {
        ui_flush();
    }
    outbuf[outn++] = ch;
    if (outn >= sizeof(outbuf)) {
        ui_flush();
    }
}

void ui_print(const char *s)
{
#ifndef A2CHAT_HOST
    __asm__("cld");
    __asm__("sei");
#endif
    while (*s) {
        ui_print_ch(*s++);
    }
    ui_flush();
}

void ui_print_n(const char *s, unsigned n)
{
    unsigned i = 0;

    ui_flush();
    sb_pin_tail();
    while (i < n) {
        char ch = s[i];
        uint8_t room;
        uint8_t take;

        if (ch == '\n' || ch == '\r') {
            ui_nl();
            i++;
            continue;
        }
        if (chat_x >= 80) {
            chat_row_done();
        }
        room = (uint8_t)(80 - chat_x);
        take = 0;
        while ((unsigned)take < room && i + take < n) {
            char c = s[i + take];
            if (c == '\n' || c == '\r') {
                break;
            }
            take++;
        }
        if (!take) {
            i++;
            continue;
        }
        chat_blit(chat_x, chat_y, s + i, take, 0);
        chat_x = (uint8_t)(chat_x + take);
        i += take;
    }
}

static void prompt_lead(void)
{
    if (chat_x == 0) {
        ui_label("You");
    }
}

void ui_prompt(char *buf, uint8_t maxlen)
{
    uint8_t n = 0;

    ui_flush();
    help_row();
    sb_pin_tail();
    prompt_lead();
    buf[0] = 0;
    cursor(1);
    for (;;) {
        char c;

        gotoxy(chat_x, chat_y);
        c = cgetc();
        if (c == '\r' || c == '\n') {
            break;
        }
        if (c == 0x0B || c == (char)CH_CURS_UP) {
            uint8_t k = open_apple_down() ? 11 : 1;
            while (k--) {
                sb_page(-1);
            }
            continue;
        }
        if (c == 0x0A || c == (char)CH_CURS_DOWN) {
            uint8_t k = open_apple_down() ? 11 : 1;
            while (k--) {
                sb_page(1);
            }
            continue;
        }
        if (sb_view) {
            sb_pin_tail();
            prompt_lead();
            if (n) {
                ui_print(buf);
            }
            continue;
        }
        if (c == 0x08 || c == 0x7f) {
            if (n) {
                n--;
                buf[n] = 0;
                if (chat_x > 0) {
                    chat_x--;
                } else if (chat_y > CHAT_TOP) {
                    chat_y--;
                    chat_x = 79;
                }
                chat_blit(chat_x, chat_y, " ", 1, 0);
            }
            continue;
        }
        if (c == 0x03) {
            buf[0] = 0;
            break;
        }
        if ((unsigned char)c >= 32 && n + 1 < maxlen) {
            buf[n++] = c;
            buf[n] = 0;
            ui_print_ch(c);
            ui_flush();
        }
    }
    ui_nl();
}

int ui_confirm(const char *path, const char *kind, unsigned bytes)
{
    char line[81];
    unsigned sec = est_secs_write(bytes);
    char c;

    ui_flush();
    sb_pin_tail();
    if (chat_x != 0) {
        ui_nl();
    }
    sprintf(line, "[%s %s  %u bytes ~%us] Y/N/E?", kind, path, bytes, sec);
    line[80] = 0;
    ui_print(line);
    gotoxy(chat_x, chat_y);
    c = (char)toupper((unsigned char)cgetc());
    ui_nl();
    if (c == 'Y') {
        return 1;
    }
    if (c == 'E') {
        return 2;
    }
    return 0;
}

char ui_getc(void)
{
    return cgetc();
}

void ui_exit(void)
{
    ui_flush();
    PAGE2_OFF = 0;
    revers(0);
    cursor(1);
}
