/*
 * Emulation of Realtek RTL8201CP PHY
 *
 * Copyright (C) 2014 Beniamino Galvani <b.galvani@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef RTL8201CP_PHY_H
#define RTL8201CP_PHY_H

#include "net/net.h"

typedef struct RTL8201CPState {
    uint16_t bmcr;
    uint16_t bmsr;
    uint16_t anar;
    uint16_t anlpar;
    NICState *nic;
} RTL8201CPState;

void rtl8201cp_phy_set_link(RTL8201CPState *mii, bool link_ok);
void rtl8201cp_phy_reset(RTL8201CPState *mii, bool link_ok);
uint16_t rtl8201cp_phy_read(RTL8201CPState *mii, uint8_t reg);
void rtl8201cp_phy_write(RTL8201CPState *mii, uint8_t reg, uint16_t value);

#endif
