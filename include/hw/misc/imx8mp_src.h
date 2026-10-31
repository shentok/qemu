/*
 * i.MX 8M Plus System Reset Controller
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FSL_IMX8MP_SRC_H
#define FSL_IMX8MP_SRC_H

#include "hw/core/sysbus.h"
#include "system/memory.h"
#include "qom/object.h"

#define TYPE_IMX8MP_SRC "fsl-imx8mp-src"
OBJECT_DECLARE_SIMPLE_TYPE(FslImx8mpSrcState, IMX8MP_SRC)

#define FSL_IMX8MP_SRC_NUM_REGS (0x100 / 4)

/* Per-core power state reported by the GPC; 1 means "domain is up". */
#define IMX8MP_SRC_CPU_POWERED "cpu-powered"

struct FslImx8mpSrcState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    /* Power state of each core, driven by the GPC */
    bool powered[4];

    /* Last CPU run state applied by the SRC, to detect level changes */
    bool running[4];

    uint32_t regs[FSL_IMX8MP_SRC_NUM_REGS];
    uint8_t boot_mode;
};

#endif /* FSL_IMX8MP_SRC_H */
