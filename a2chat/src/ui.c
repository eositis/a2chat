#include "a2chat.h"
#include <apple2enh.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <apple2.h>
#include <ip65.h>

#define CHROME_TOP 0
#define CHAT_TOP 2
#define CHAT_BOT 20
#define INPUT_ROW 22
#define HELP_ROW 23

#define STORE80_ON  (*(volatile unsigned char *)0xC001)
#define PAGE2_OFF   (*(volatile unsigned char *)0xC054)
#define PAGE2_ON    (*(volatile unsigned char *)0xC055)

static uint8_t chat_y = CHAT_TOP;
static uint8_t chat_x = 0;

uint8_t g_slot;
char g_status[81];

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

static unsigned text_base(uint8_t row)
{
    return 0x400u + (unsigned)((row & 7) * 0x80 + (row >> 3) * 40);
}

static void chat_plot(uint8_t col, uint8_t row, char ch, uint8_t inverse)
{
    unsigned char c = (unsigned char)ch & 0x7f;
    unsigned addr;

    if (c < 32) {
        c = ' ';
    }
    if (inverse) {
        if (c >= 'a' && c <= 'z') {
            c = (unsigned char)(c - 32);
        }
        c = (unsigned char)(c & 0x3f);
    } else {
        c = (unsigned char)(c | 0x80);
    }
    addr = text_base(row) + (col >> 1);
    STORE80_ON = 0;
    /* IIe 80-col: even columns in aux, odd in main. */
    if (col & 1) {
        PAGE2_OFF = 0;
    } else {
        PAGE2_ON = 0;
    }
    *(volatile unsigned char *)(unsigned)addr = c;
    PAGE2_OFF = 0;
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

void ui_redraw_chrome(void)
{
    char line[81];
    gotoxy(0, CHROME_TOP);
    revers(1);
    sprintf(line, "A2CHAT  %s:%u  %-12s  Slot%u  [%s]",
            g_cfg.host, (unsigned)g_cfg.port, g_cfg.model,
            (unsigned)g_slot, g_net_ok ? "Conn" : "----");
    line[80] = 0;
    cputs(line);
    cclear(80 - (unsigned char)strlen(line));
    revers(0);
    gotoxy(0, 1);
    chline(80);
    gotoxy(0, 21);
    chline(80);
    gotoxy(0, HELP_ROW);
    cputs("OA-C cfg  /ping  /cat /new /model  ESC abort  OA-Q quit");
}

void ui_init(void)
{
    videomode(VIDEOMODE_80COL);
    clrscr();
    cursor(1);
    chat_y = CHAT_TOP;
    chat_x = 0;
    ui_redraw_chrome();
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

void ui_nl(void)
{
    chat_x = 0;
    if (chat_y < CHAT_BOT) {
        chat_y++;
    } else {
        chat_y = CHAT_TOP;
    }
    chat_clear_line(chat_y);
}

void ui_label(const char *s)
{
    if (chat_x != 0) {
        ui_nl();
    }
    while (*s) {
        chat_plot(chat_x, chat_y, *s, 1);
        chat_x++;
        s++;
        if (chat_x >= 80) {
            ui_nl();
        }
    }
    chat_plot(chat_x, chat_y, ' ', 0);
    chat_x++;
}

void ui_print_ch(char ch)
{
    if (ch == '\n' || ch == '\r') {
        ui_nl();
        return;
    }
    if (ch == '\t') {
        ch = ' ';
    }
    chat_plot(chat_x, chat_y, ch, 0);
    chat_x++;
    if (chat_x >= 80) {
        ui_nl();
    }
}

void ui_print(const char *s)
{
    while (*s) {
        ui_print_ch(*s++);
    }
}

void ui_prompt(char *buf, uint8_t maxlen)
{
    uint8_t n = 0;
    gotoxy(0, INPUT_ROW);
    cclear(80);
    gotoxy(0, INPUT_ROW);
    cputs("> ");
    buf[0] = 0;
    for (;;) {
        char c = cgetc();
        if (c == '\r' || c == '\n') {
            break;
        }
        if (c == 0x08 || c == 0x7f) {
            if (n) {
                n--;
                buf[n] = 0;
                gotoxy((unsigned char)(2 + n), INPUT_ROW);
                cputc(' ');
                gotoxy((unsigned char)(2 + n), INPUT_ROW);
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
            cputc(c);
        }
    }
}

int ui_confirm(const char *path, const char *kind, unsigned bytes)
{
    char line[81];
    unsigned sec = est_secs_write(bytes);
    char c;

    gotoxy(0, INPUT_ROW);
    cclear(80);
    gotoxy(0, INPUT_ROW);
    sprintf(line, "[%s %s  %u bytes ~%us] Y/N/E?", kind, path, bytes, sec);
    line[80] = 0;
    cputs(line);
    c = (char)toupper((unsigned char)cgetc());
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
