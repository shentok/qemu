/*
 * i.MX 8M On-Chip OTP Controller
 *
 * Copyright (c) 2026 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IMX8M_OCOTP_H
#define IMX8M_OCOTP_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_IMX8M_OCOTP "imx8m-ocotp"
OBJECT_DECLARE_SIMPLE_TYPE(IMX8MOcotpState, IMX8M_OCOTP)

struct IMX8MOcotpState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    BlockBackend *blk;

    uint32_t ctrl;
    uint32_t timing;
    uint32_t data;
    uint32_t read_ctrl;
    uint32_t read_fuse_data;
    uint32_t sw_sticky;
    uint32_t scs;

    /* Fuses and their shadow registers, one word each */
    uint32_t *fuses;
    uint32_t *shadow;
    uint32_t num_words;

    /* Properties */
    uint32_t num_banks;
    bool imx8mp;
};

#endif /* IMX8M_OCOTP_H */
