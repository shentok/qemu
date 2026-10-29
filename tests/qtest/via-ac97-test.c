/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * QTest testcase for the VIA VT82C686 AC'97 SGD engine
 *
 * The Linux vIA82xx driver documents CURR_PTR as "the descriptor address
 * plus 8" and computes the descriptor being played as
 *
 *     idx = ((CURR_PTR - table) / 8 - 1) % entries
 *
 * The engine must therefore expose the *next* descriptor while the current
 * one is being played, and it must keep playing across a period boundary
 * instead of stalling until the guest acknowledges the interrupt.
 *
 * This work is licensed under the GNU GPL license version 2 or later.
 */

#include "qemu/osdep.h"
#include "libqtest.h"

#define VIA_VENDOR_ID       0x1106
#define VIA_AC97_DEVICE_ID  0x3058

#define SGD_STATUS          0x00
#define SGD_CONTROL         0x01
#define SGD_TYPE            0x02
#define SGD_TABLE_ADDR      0x04
#define SGD_CURR_COUNT      0x0c
#define SGD_SHADOW          0x84

#define CTRL_START          0x80

#define TYPE_AUTOSTART      0x80
#define TYPE_16BIT          0x20
#define TYPE_STEREO         0x10
#define TYPE_INT_EOL        0x02
#define TYPE_INT_FLAG       0x01

#define DESC_FLAG           0x40000000
#define DESC_EOL            0x80000000

#define TABLE_ADDR          0x00100000
#define DATA_ADDR           0x00200000
#define PERIOD_BYTES        1024
#define PERIODS             4

#define SHADOW_FLAG         (1u << 0)
#define SHADOW_EOL          (1u << 4)
#define SHADOW_ACTIVE       (1u << 12)

static uint32_t pci_config_read32(QTestState *s, int devfn, int offset)
{
    qtest_outl(s, 0xcf8, 0x80000000u | (devfn << 8) | (offset & 0xfc));
    return qtest_inl(s, 0xcfc);
}

static void pci_config_write32(QTestState *s, int devfn, int offset,
                               uint32_t value)
{
    qtest_outl(s, 0xcf8, 0x80000000u | (devfn << 8) | (offset & 0xfc));
    qtest_outl(s, 0xcfc, value);
}

static int find_ac97(QTestState *s)
{
    int devfn;

    for (devfn = 0; devfn < 256; devfn++) {
        if (pci_config_read32(s, devfn, 0) ==
            ((VIA_AC97_DEVICE_ID << 16) | VIA_VENDOR_ID)) {
            return devfn;
        }
    }
    return -1;
}

static void test_sgd_pointer(void)
{
    QTestState *s = qtest_init(
        "-M pc,x-south-bridge=vt82c686b-isa,x-north-bridge=vt82c694t,"
        "audiodev=snd0 -audiodev none,id=snd0");
    uint32_t table[PERIODS * 2];
    uint32_t bar;
    uint32_t cmd;
    int devfn;
    int i;

    devfn = find_ac97(s);
    g_assert_cmpint(devfn, >=, 0);

    cmd = pci_config_read32(s, devfn, 0x04);
    /* Assign an I/O BAR address ourselves; qtest has no firmware for that. */
    pci_config_write32(s, devfn, 0x10, 0xc001);
    pci_config_write32(s, devfn, 0x04, cmd | 1);
    bar = pci_config_read32(s, devfn, 0x10) & ~3u;
    g_assert_cmphex(bar, !=, 0);

    for (i = 0; i < PERIODS; i++) {
        table[i * 2] = DATA_ADDR + i * PERIOD_BYTES;
        table[i * 2 + 1] = PERIOD_BYTES |
            (i == PERIODS - 1 ? DESC_EOL : DESC_FLAG);
    }
    qtest_memwrite(s, TABLE_ADDR, table, sizeof(table));
    qtest_memset(s, DATA_ADDR, 0, PERIODS * PERIOD_BYTES);

    qtest_outl(s, bar + SGD_TABLE_ADDR, TABLE_ADDR);
    qtest_outb(s, bar + SGD_TYPE,
               TYPE_AUTOSTART | TYPE_16BIT | TYPE_STEREO |
               TYPE_INT_EOL | TYPE_INT_FLAG);
    qtest_outb(s, bar + SGD_CONTROL, CTRL_START);

    /*
     * Starting the engine must load the first descriptor immediately, so
     * that a guest reading the pointer before the first audio timer tick
     * gets the descriptor that is being played rather than a stale one.
     */
    g_assert_cmphex(qtest_inl(s, bar + SGD_TABLE_ADDR), ==, TABLE_ADDR + 8);
    g_assert_cmphex(qtest_inl(s, bar + SGD_CURR_COUNT), ==, PERIOD_BYTES);

    /* Three audio timer ticks (10 ms each) cross several period ends. */
    qtest_clock_step(s, 30 * 1000000LL);

    /*
     * Nothing has acknowledged the FLAG/IRQ, yet the engine must have kept
     * playing: the paused bit in the control/status register stays clear
     * and the shadow status reports the interrupts that fired.
     */
    g_assert_cmphex(qtest_inb(s, bar + SGD_CONTROL) & 0x08, ==, 0);
    g_assert_cmphex(qtest_inl(s, bar + SGD_SHADOW) & SHADOW_ACTIVE,
                    ==, SHADOW_ACTIVE);
    g_assert_cmphex(qtest_inl(s, bar + SGD_SHADOW) &
                    (SHADOW_FLAG | SHADOW_EOL), !=, 0);

    qtest_quit(s);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    qtest_add_func("/via-ac97/sgd-pointer", test_sgd_pointer);
    return g_test_run();
}
