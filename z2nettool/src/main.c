#include <stdlib.h>
#include <string.h>
#include "a2net.h"

static void quit_to_prodos(void)
{
    ui_cls();
    ui_puts(10, 8, "RETURNING TO PRODOS", 0);
    exit(0);
}

shared_t g_share;
unsigned char g_tool;
unsigned char g_focus;
unsigned char g_cur;
unsigned char g_nfields;
unsigned char g_in_output;
unsigned char g_busy;
field_t g_fields[6];

static void defaults(void)
{
    memset(&g_share, 0, sizeof(g_share));
    strcpy(g_share.count, "4");
    strcpy(g_share.timeout, "2000");
    strcpy(g_share.max_hops, "16");
    strcpy(g_share.port, "80");
    strcpy(g_share.proto, "tcp");
    strcpy(g_share.ports, "22,80,443");
    strcpy(g_share.server, "");
    strcpy(g_share.wserver, "whois.iana.org");
    if (g_dhcp_ok)
        strcpy(g_share.host, dotted_quad(cfg_gateway));
}

static void show(void)
{
    if (g_tool == TOOL_MENU)
        ui_draw_menu();
    else
        ui_draw_tool();
}

static void draw_status(void)
{
    const char *s;
    if (g_in_output)
        s = "UP/DN scroll output   RET run";
    else if (g_nfields && g_focus < g_nfields)
        s = g_fields[g_focus].help;
    else
        s = "RET run   OA hotkey switches";
    ui_draw_help(s);
}

static void switch_tool(unsigned char tool)
{
    if (tool == g_tool)
        return;
    g_tool = tool;
    if (tool == TOOL_MENU) {
        g_nfields = 0;
        g_in_output = 0;
        show();
        return;
    }
    ui_setup_tool(tool);
    show();
}

static unsigned char letter_tool(unsigned char k)
{
    if (k >= 'a' && k <= 'z')
        k = (unsigned char)(k - 0x20);
    switch (k) {
    case 'P':
        return TOOL_PING;
    case 'T':
        return TOOL_TRACE;
    case 'N':
        return TOOL_NS;
    case 'W':
        return TOOL_WHOIS;
    case 'C':
        return TOOL_NC;
    case 'M':
        return TOOL_NMAP;
    case 'Q':
        return TOOL_MENU;
    default:
        return 0xFF;
    }
}

static unsigned char ctrl_tool(unsigned char k)
{
    switch (k) {
    case 0x10: /* Ctrl-P */
        return TOOL_PING;
    case 0x14: /* Ctrl-T */
        return TOOL_TRACE;
    case 0x0E: /* Ctrl-N */
        return TOOL_NS;
    case 0x17: /* Ctrl-W */
        return TOOL_WHOIS;
    case 0x0C: /* Ctrl-L  nc (Ctrl-C is interrupt) */
        return TOOL_NC;
    case 0x06: /* Ctrl-F  nmap (Ctrl-M is CR) */
        return TOOL_NMAP;
    case 0x11: /* Ctrl-Q */
        return TOOL_MENU;
    case 0x18: /* Ctrl-X quit */
        quit_to_prodos();
        return 0xFF;
    default:
        return 0xFF;
    }
}

static void run_current(void)
{
    if (g_tool == TOOL_MENU)
        return;
    if (!g_nic_ok) {
        ui_log("no NIC");
        return;
    }
    g_busy = 1;
    ui_draw_help("ESC abort   running...");
    switch (g_tool) {
    case TOOL_PING:
        ping_run();
        break;
    case TOOL_TRACE:
        traceroute_run();
        break;
    case TOOL_NS:
        nslookup_run();
        break;
    case TOOL_WHOIS:
        whois_run();
        break;
    case TOOL_NC:
        nc_run();
        break;
    case TOOL_NMAP:
        nmap_run();
        break;
    }
    g_busy = 0;
    ui_draw_fields();
    draw_status();
}

static void handle_nav(unsigned char k)
{
    if (k == 0x09) { /* tab */
        if (g_in_output) {
            g_in_output = 0;
            g_focus = 0;
            g_cur = (unsigned char)strlen(g_fields[0].buf);
        } else if (g_focus + 1 < g_nfields) {
            g_focus++;
            g_cur = (unsigned char)strlen(g_fields[g_focus].buf);
        } else {
            g_in_output = 1;
        }
        ui_draw_fields();
        draw_status();
        return;
    }
    if (k == 0x0B) { /* up */
        if (g_in_output) {
            ui_scroll(-1);
            return;
        }
        if (g_focus) {
            g_focus--;
            g_cur = (unsigned char)strlen(g_fields[g_focus].buf);
            ui_draw_fields();
            ui_draw_help(g_fields[g_focus].help);
        }
        return;
    }
    if (k == 0x0A) { /* down */
        if (g_in_output) {
            ui_scroll(1);
            return;
        }
        if (g_focus + 1 < g_nfields) {
            g_focus++;
            g_cur = (unsigned char)strlen(g_fields[g_focus].buf);
            ui_draw_fields();
            ui_draw_help(g_fields[g_focus].help);
        } else {
            g_in_output = 1;
            ui_draw_fields();
            ui_draw_help("UP/DN scroll output   RET run");
        }
    }
}

int main(void)
{
    unsigned char k, t, oa;

    net_init();
    defaults();
    g_tool = TOOL_MENU;
    show();

    for (;;) {
        ip65_process();
        if (!key_poll(&k))
            continue;
        oa = key_oa();

        t = ctrl_tool(k);
        if (t != 0xFF) {
            switch_tool(t);
            continue;
        }
        if (oa) {
            if (k == 'X' || k == 'x') {
                quit_to_prodos();
                continue;
            }
            t = letter_tool(k);
            if (t != 0xFF) {
                switch_tool(t);
                continue;
            }
        }

        if (g_tool == TOOL_MENU) {
            if (k == '7' || k == 'X' || k == 'x' || k == 'Q' || k == 'q') {
                quit_to_prodos();
                continue;
            }
            if (k >= '1' && k <= '6') {
                switch_tool((unsigned char)(k - '0'));
                continue;
            }
            t = letter_tool(k);
            if (t != 0xFF) {
                switch_tool(t);
                continue;
            }
            continue;
        }

        if (k == 0x0D) {
            run_current();
            continue;
        }
        if (k == 0x1B) {
            if (!g_busy)
                switch_tool(TOOL_MENU);
            continue;
        }

        if (k == 0x09 || k == 0x0A || k == 0x0B) {
            handle_nav(k);
            continue;
        }
        ui_handle_key(k);
    }
    return 0;
}
