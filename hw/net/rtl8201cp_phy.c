/*
 * Emulation of Realtek RTL8201CP PHY
 *
 * Copyright (C) 2014 Beniamino Galvani <b.galvani@gmail.com>
 *
 * This model is based on reverse-engineering of Linux kernel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include "qemu/osdep.h"
#include "hw/net/rtl8201cp_phy.h"
#include "qemu/log.h"
#include "hw/net/mii.h"

void rtl8201cp_phy_set_link(RTL8201CPState *mii, bool link_ok)
{
    if (link_ok) {
        mii->bmsr |= MII_BMSR_LINK_ST | MII_BMSR_AN_COMP;
        mii->anlpar |= MII_ANAR_TXFD | MII_ANAR_10FD | MII_ANAR_10 |
                       MII_ANAR_CSMACD;
    } else {
        mii->bmsr &= ~(MII_BMSR_LINK_ST | MII_BMSR_AN_COMP);
        mii->anlpar = MII_ANAR_TX;
    }
}

void rtl8201cp_phy_reset(RTL8201CPState *mii, bool link_ok)
{
    mii->bmcr = MII_BMCR_FD | MII_BMCR_AUTOEN | MII_BMCR_SPEED;
    mii->bmsr = MII_BMSR_100TX_FD | MII_BMSR_100TX_HD | MII_BMSR_10T_FD |
                MII_BMSR_10T_HD | MII_BMSR_MFPS | MII_BMSR_AUTONEG;
    mii->anar = MII_ANAR_TXFD | MII_ANAR_TX | MII_ANAR_10FD | MII_ANAR_10 |
                MII_ANAR_CSMACD;
    mii->anlpar = MII_ANAR_TX;

    rtl8201cp_phy_set_link(mii, link_ok);
}

uint16_t rtl8201cp_phy_read(RTL8201CPState *mii, uint8_t reg)
{
    switch (reg) {
    case MII_BMCR:
        return mii->bmcr;
    case MII_BMSR:
        return mii->bmsr;
    case MII_PHYID1:
        return RTL8201CP_PHYID1;
    case MII_PHYID2:
        return RTL8201CP_PHYID2;
    case MII_ANAR:
        return mii->anar;
    case MII_ANLPAR:
        return mii->anlpar;
    case MII_ANER:
    case MII_NSR:
    case MII_LBREMR:
    case MII_REC:
    case MII_SNRDR:
    case MII_TEST:
        qemu_log_mask(LOG_UNIMP, "%s: read from unimpl. mii reg 0x%x\n",
                      __func__, reg);
        return 0;
    }

    qemu_log_mask(LOG_GUEST_ERROR, "%s: read from invalid mii reg 0x%x\n",
                  __func__, reg);
    return 0;
}

void rtl8201cp_phy_write(RTL8201CPState *mii, uint8_t reg, uint16_t value)
{
    NetClientState *nc;

    switch (reg) {
    case MII_BMCR:
        if (value & MII_BMCR_RESET) {
            nc = qemu_get_queue(mii->nic);
            rtl8201cp_phy_reset(mii, !nc->link_down);
        } else {
            mii->bmcr = value;
        }
        break;
    case MII_ANAR:
        mii->anar = value;
        break;
    case MII_BMSR:
    case MII_PHYID1:
    case MII_PHYID2:
    case MII_ANLPAR:
    case MII_ANER:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: write to read-only mii reg 0x%x\n",
                      __func__, reg);
        break;
    case MII_NSR:
    case MII_LBREMR:
    case MII_REC:
    case MII_SNRDR:
    case MII_TEST:
        qemu_log_mask(LOG_UNIMP, "%s: write to unimpl. mii reg 0x%x\n",
                      __func__, reg);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: write to invalid mii reg 0x%x\n",
                      __func__, reg);
    }
}
