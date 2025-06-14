/*
 * QEMU Freescale eTSEC Emulator
 *
 * Copyright (c) 2011-2013 AdaCore
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/net/mii.h"
#include "etsec.h"
#include "registers.h"
#include "../trace.h"

uint16_t fsl_etsec_phy_read(EtsecPhyState *s, uint8_t addr)
{
    uint16_t value;

    switch (addr) {
    case MII_BMCR:
        value = s->phy_control;
        break;
    case MII_BMSR:
        value = s->phy_status;
        break;
    case MII_STAT1000:
        value = MII_STAT1000_LOK | MII_STAT1000_ROK;
        break;
    default:
        value = 0x0;
        break;
    };

    return value;
}

void fsl_etsec_phy_write(EtsecPhyState *s, uint8_t addr, uint16_t value)
{
    switch (addr) {
    case MII_BMCR:
        s->phy_control = value & ~(MII_BMCR_RESET | MII_BMCR_FD);
        break;
    default:
        break;
    };
}

void fsl_etsec_phy_reset(EtsecPhyState *s)
{
    s->phy_status =
        MII_BMSR_EXTCAP   | MII_BMSR_LINK_ST  | MII_BMSR_AUTONEG  |
        MII_BMSR_AN_COMP  | MII_BMSR_MFPS     | MII_BMSR_EXTSTAT  |
        MII_BMSR_100T2_HD | MII_BMSR_100T2_FD |
        MII_BMSR_10T_HD   | MII_BMSR_10T_FD   |
        MII_BMSR_100TX_HD | MII_BMSR_100TX_FD | MII_BMSR_100T4;
}

void fsl_etsec_phy_set_link_status(EtsecPhyState *s, bool link_down)
{
    /* Set link status */
    if (link_down) {
        s->phy_status &= ~MII_BMSR_LINK_ST;
    } else {
        s->phy_status |= MII_BMSR_LINK_ST;
    }
}
