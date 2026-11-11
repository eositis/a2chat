#include "a2chat.h"
#include "nic.h"
#include <stdio.h>
#include <string.h>

extern uint8_t nic_hole[];

static struct nic *vec;

static int load_file(const char *name)
{
    FILE *f;
    unsigned n;
    struct nic *nvec;
    int extra;

    vec = 0;
    f = fopen(name, "rb");
    if (!f) {
        ui_print(name);
        ui_print(" not on disk");
        ui_nl();
        return -1;
    }
    memset(nic_hole, 0, NIC_HOLE_SIZE);
    n = fread(nic_hole, 1, NIC_HOLE_SIZE, f);
    extra = (n == NIC_HOLE_SIZE) ? fgetc(f) : EOF;
    fclose(f);
    if (extra != EOF) {
        ui_print(name);
        ui_print(" is too big");
        ui_nl();
        return -1;
    }
    nvec = (struct nic *)nic_hole;
    if (n < 8 || nvec->magic[0] != 'N' || nvec->load_size != n) {
        ui_print(name);
        ui_print(" is not a NIC overlay");
        ui_nl();
        return -1;
    }
    vec = nvec;
    return 0;
}

static uint8_t find_uther(uint8_t configured)
{
    uint8_t prefer;
    uint8_t s;
    uint8_t mf;
    uint8_t (*probe)(uint8_t);

    /* The W5100 probe writes the mode register and writes back whatever
     * it read. MegaFlash does not read that register back, so the restore
     * crashes the machine. The Uthernet II is in the MegaFlash slot. */
    mf = slot_megaflash();
    if (mf) {
        ui_print("MegaFlash slot 4");
        ui_nl();
        return mf;
    }
    probe = vec->probe;
    if (configured >= 1 && configured <= 7) {
        return slot_is_disk2(configured) ? 0 : configured;
    }
    prefer = slot_prefer_uther();
    if (!slot_is_disk2(prefer) && probe(prefer)) {
        return prefer;
    }
    for (s = 1; s <= 7; s++) {
        if (s == prefer || slot_is_disk2(s)) {
            continue;
        }
        if (probe(s)) {
            return s;
        }
    }
    return prefer;
}

int nic_bringup(void)
{
    uint8_t slot;
    int rc;

    g_net_ok = 0;
    g_slot = 0;
    vec = 0;
    slot = slot_find_cogs(g_cfg.slot);
    if (slot) {
        if (load_file("A2CHAT.COG") != 0) {
            return -1;
        }
    } else {
        if (g_cfg.slot >= 1 && g_cfg.slot <= 7 && slot_is_disk2(g_cfg.slot)) {
            ui_print("SLOT= is a Disk II");
            ui_nl();
            return -1;
        }
        if (load_file("A2CHAT.UTH") != 0) {
            return -1;
        }
        slot = find_uther(g_cfg.slot);
        if (!slot) {
            ui_print("No network slot");
            ui_nl();
            return -1;
        }
    }
    g_slot = slot;
    rc = vec->init(slot);
    if (rc != 0) {
        g_net_ok = 0;
    }
    return rc;
}
