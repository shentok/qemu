/*
 * i.MX IC Identification Module (IIM)
 *
 * Copyright (c) 2026 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IMX_IIM_H
#define IMX_IIM_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_IMX_IIM "imx-iim"
OBJECT_DECLARE_SIMPLE_TYPE(IMXIIMState, IMX_IIM)

#define IMX_IIM_ROWS_PER_BANK 32

struct IMXIIMState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    BlockBackend *blk;

    uint32_t stat;
    uint32_t statm;
    uint32_t err;
    uint32_t emask;
    uint32_t fctl;
    uint32_t ua;
    uint32_t la;
    uint32_t sdat;
    uint32_t prg_p;
    uint32_t scs[4];

    /* Fuses and the values read via the bank registers, one row each */
    uint8_t *fuses;
    uint8_t *shadow;
    uint32_t num_rows;

    /* Properties */
    uint32_t num_banks;
    uint8_t prev;
    uint8_t srev;
};

#endif /* IMX_IIM_H */
